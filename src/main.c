#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "core.h"
#include "network.h"
#include "ui.h"

#define GRAVITY_MS      500u
#define TICK_MS          16u
#define LOCK_DELAY_MS   500u
#define MAX_LOCK_RESETS  15
#define DEFAULT_PORT   5555u

/* DAS/ARR — OS 키 리피트가 상한을 결정하므로 이 값은 '그 이상의 throttle' 역할만 함 */
#define DAS_MS           150u
#define ARR_MS            30u
#define HOLD_RELEASE_MS  100u

typedef struct {
    bool     held;
    uint64_t firstPress;
    uint64_t lastInput;
    uint64_t lastShift;
} KeyHold;

typedef bool (*MoveFn)(GameState*);

static bool handleDirHold(KeyHold* h, uint64_t now, MoveFn move, GameState* state)
{
    bool moved = false;
    h->lastInput = now;
    if (!h->held) {
        if (move(state)) moved = true;
        h->held = true;
        h->firstPress = now;
        h->lastShift = now;
        return moved;
    }
    if (now - h->firstPress >= DAS_MS) {
        if (now - h->lastShift >= ARR_MS) {
            if (move(state)) moved = true;
            h->lastShift = now;
        }
    }
    return moved;
}

static uint64_t nowMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void msleep(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* TETR.IO Combo 보너스 테이블 (combo 카운터 인덱스) */
static uint8_t comboBonus(uint16_t combo)
{
    static const uint8_t table[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
    if (combo == 0) return 0;
    uint16_t idx = combo - 1;
    if (idx >= sizeof(table) / sizeof(table[0])) return 5;
    return table[idx];
}

/* TETR.IO 공격 라인 계산 */
static uint8_t computeAttack(int lines, TSpinType tspin, bool perfectClear,
                             bool b2bActive, uint16_t combo)
{
    int base = 0;
    bool difficult = false;

    if (tspin == TSPIN_FULL) {
        switch (lines) {
            case 1: base = 2; difficult = true; break;
            case 2: base = 4; difficult = true; break;
            case 3: base = 6; difficult = true; break;
            default: break;
        }
    } else if (tspin == TSPIN_MINI) {
        switch (lines) {
            case 1: base = 0; difficult = true; break;
            case 2: base = 1; difficult = true; break;
            default: break;
        }
    } else {
        switch (lines) {
            case 1: base = 0; break;
            case 2: base = 1; break;
            case 3: base = 2; break;
            case 4: base = 4; difficult = true; break;
            default: break;
        }
    }

    int bonus = 0;
    if (difficult && b2bActive) bonus += 1;
    bonus += (int)comboBonus(combo);
    if (perfectClear && lines > 0) bonus += 10;

    int total = base + bonus;
    if (total < 0) total = 0;
    if (total > 255) total = 255;
    return (uint8_t)total;
}

/* TETR.IO 스코어 (간이판) */
static uint32_t computeScore(int lines, TSpinType tspin, bool perfectClear,
                             bool b2bBefore, bool stillDifficult, uint16_t comboAfter)
{
    uint32_t s = 0;
    if (tspin == TSPIN_FULL) {
        switch (lines) {
            case 0: s = 400; break;
            case 1: s = 800; break;
            case 2: s = 1200; break;
            case 3: s = 1600; break;
        }
    } else if (tspin == TSPIN_MINI) {
        switch (lines) {
            case 0: s = 100; break;
            case 1: s = 200; break;
            case 2: s = 400; break;
        }
    } else {
        switch (lines) {
            case 1: s = 100; break;
            case 2: s = 300; break;
            case 3: s = 500; break;
            case 4: s = 800; break;
        }
    }
    if (stillDifficult && b2bBefore && lines > 0) {
        s = (s * 3) / 2;            /* B2B 1.5x */
    }
    if (comboAfter >= 2 && lines > 0) {
        s += 50u * (comboAfter - 1);
    }
    if (perfectClear && lines > 0) {
        switch (lines) {
            case 1: s += 800; break;
            case 2: s += 1200; break;
            case 3: s += 1800; break;
            case 4: s += 2000; break;
        }
    }
    return s;
}

static void printUsage(const char* prog)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s                       single-player\n", prog);
    fprintf(stderr, "  %s host [port]           host a 1v1 game (default port %u)\n",
            prog, DEFAULT_PORT);
    fprintf(stderr, "  %s join <ip> [port]      join a 1v1 game\n", prog);
}

int main(int argc, char** argv)
{
    NetContext net;
    memset(&net, 0, sizeof(net));
    net.sock = -1;

    NetMode mode = NET_MODE_NONE;
    const char* host = NULL;
    uint16_t port = (uint16_t)DEFAULT_PORT;

    if (argc >= 2) {
        if (strcmp(argv[1], "host") == 0) {
            mode = NET_MODE_HOST;
            if (argc >= 3) port = (uint16_t)atoi(argv[2]);
        } else if (strcmp(argv[1], "join") == 0) {
            if (argc < 3) { printUsage(argv[0]); return 1; }
            mode = NET_MODE_CLIENT;
            host = argv[2];
            if (argc >= 4) port = (uint16_t)atoi(argv[3]);
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            printUsage(argv[0]);
            return 1;
        }
    }

    uint32_t seed = 0;
    if (mode == NET_MODE_HOST) {
        if (netHost(&net, port) != 0) { fprintf(stderr, "Failed to host.\n"); return 1; }
        if (netExchangeSeed(&net) != 0) { fprintf(stderr, "Seed exchange failed.\n"); netClose(&net); return 1; }
        seed = net.seed;
        fprintf(stderr, "Opponent connected. Seed=%u\n", seed);
    } else if (mode == NET_MODE_CLIENT) {
        fprintf(stderr, "Connecting to %s:%u...\n", host, port);
        if (netJoin(&net, host, port) != 0) { fprintf(stderr, "Failed to connect.\n"); return 1; }
        if (netExchangeSeed(&net) != 0) { fprintf(stderr, "Seed exchange failed.\n"); netClose(&net); return 1; }
        seed = net.seed;
        fprintf(stderr, "Connected. Seed=%u\n", seed);
    } else {
        seed = (uint32_t)time(NULL);
    }

    srand(seed);

    GameState state;
    initGame(&state, seed);

    uiInit();

    uint64_t lastGravity = nowMs();
    bool grounded = false;
    uint64_t groundedSince = 0;
    int resetsLeft = MAX_LOCK_RESETS;
    int lastActivePieceTag = -1;

    KeyHold leftH  = {0};
    KeyHold rightH = {0};
    KeyHold downH  = {0};

    while (!state.isGameOver) {
        bool didLock = false;
        bool actedSuccessfully = false;

        /* 새 피스로 바뀌었는지 추적 (간이 태그) */
        int curTag = (int)state.activeBlock.type * 10000 +
                     (int)state.activeBlock.x * 100 +
                     (int)state.activeBlock.y;
        if (curTag != lastActivePieceTag) {
            grounded = false;
            resetsLeft = MAX_LOCK_RESETS;
            lastActivePieceTag = curTag;
        }

        uint64_t inputNow = nowMs();

        /* 입력 소진 — DAS/ARR 적용 */
        for (;;) {
            UiKey key = uiPollKey();
            if (key == UI_KEY_NONE) break;
            bool ok = false;
            bool doHardDrop = false;
            bool didHold = false;
            switch (key) {
                case UI_KEY_LEFT:
                    ok = handleDirHold(&leftH, inputNow, moveLeft, &state);
                    rightH.held = false;
                    break;
                case UI_KEY_RIGHT:
                    ok = handleDirHold(&rightH, inputNow, moveRight, &state);
                    leftH.held = false;
                    break;
                case UI_KEY_SOFT_DROP:
                    ok = handleDirHold(&downH, inputNow, moveDown, &state);
                    break;
                case UI_KEY_ROTATE_CW:  ok = rotateCW(&state); break;
                case UI_KEY_ROTATE_CCW: ok = rotateCCW(&state); break;
                case UI_KEY_ROTATE_180: ok = rotate180(&state); break;
                case UI_KEY_HARD_DROP:  hardDropToBottom(&state); doHardDrop = true; break;
                case UI_KEY_HOLD:
                    {
                        BlockType prevHold = state.holdBlock;
                        bool prevCanHold = state.canHold;
                        holdCurrentBlock(&state);
                        if (prevCanHold && state.holdBlock != prevHold) {
                            didHold = true;
                        }
                    }
                    break;
                case UI_KEY_QUIT:       state.isGameOver = true; break;
                default: break;
            }
            if (ok) actedSuccessfully = true;
            if (didHold && net.mode != NET_MODE_NONE && net.connected) {
                netSendHold(&net, &state);
            }
            if (doHardDrop) { didLock = true; break; }
            if (state.isGameOver) break;
        }

        /* DAS/ARR release detection (no recent input → unhold) */
        if (leftH.held  && inputNow - leftH.lastInput  > HOLD_RELEASE_MS) leftH.held  = false;
        if (rightH.held && inputNow - rightH.lastInput > HOLD_RELEASE_MS) rightH.held = false;
        if (downH.held  && inputNow - downH.lastInput  > HOLD_RELEASE_MS) downH.held  = false;

        /* gravity */
        uint64_t now = nowMs();
        if (!state.isGameOver && !didLock && now - lastGravity >= GRAVITY_MS) {
            lastGravity = now;
            if (moveDown(&state)) actedSuccessfully = true;
        }

        /* grounded 상태 갱신 */
        if (!state.isGameOver && !didLock) {
            bool canDown = !checkCollision(&state,
                                           state.activeBlock.x,
                                           state.activeBlock.y + 1,
                                           state.activeBlock.rotation);
            if (canDown) {
                grounded = false;
            } else {
                if (!grounded) {
                    grounded = true;
                    groundedSince = now;
                } else if (actedSuccessfully && resetsLeft > 0) {
                    resetsLeft--;
                    groundedSince = now;
                }
                if (now - groundedSince >= LOCK_DELAY_MS) {
                    didLock = true;
                }
            }
        }

        /* lock 처리 */
        if (didLock && !state.isGameOver) {
            TSpinType tspin = detectTSpin(&state);
            int lines = lockBlock(&state);
            bool pc = (lines > 0) && isBoardEmpty(&state);

            bool b2bBefore = (state.b2b > 0);
            bool difficult = (lines == 4) ||
                             (tspin != TSPIN_NONE && lines > 0);

            uint16_t newCombo;
            if (lines == 0) newCombo = 0;
            else            newCombo = state.combo + 1;

            uint8_t attack = computeAttack(lines, tspin, pc, b2bBefore, newCombo);
            uint32_t scoreAdd = computeScore(lines, tspin, pc, b2bBefore, difficult, newCombo);

            state.score      += scoreAdd;
            state.totalLines += (uint32_t)lines;
            state.combo       = newCombo;
            if (lines > 0) {
                state.b2b = difficult ? (uint8_t)(state.b2b + 1) : 0;
            }

            /* garbage 큐 상쇄 */
            uint8_t toSend = attack;
            if (state.pendingGarbage > 0 && toSend > 0) {
                uint8_t cancel = (toSend < state.pendingGarbage) ? toSend : state.pendingGarbage;
                state.pendingGarbage = (uint8_t)(state.pendingGarbage - cancel);
                toSend = (uint8_t)(toSend - cancel);
            }

            if (net.mode != NET_MODE_NONE) {
                netSendLock(&net, &state, toSend);
            }

            spawnBlock(&state);
            grounded = false;
            resetsLeft = MAX_LOCK_RESETS;
            lastGravity = nowMs();

            /* 새 피스에는 이전 hold 상태 이월 안 함 — 직접 누른 키만 인정 */
            leftH.held = rightH.held = downH.held = false;
        }

        /* 활성 블록 동기화 */
        if (net.mode != NET_MODE_NONE && net.connected && !state.isGameOver) {
            netSendState(&net, &state.activeBlock);
        }

        /* 상대 폴링 */
        if (net.mode != NET_MODE_NONE) {
            netPoll(&net, &state);
            if (net.opponentLost) break;
            if (!net.connected) break;
        }

        uiRender(&state, (net.mode == NET_MODE_NONE) ? NULL : &net);

        msleep(TICK_MS);
    }

    bool sentOver = false;
    if (net.mode != NET_MODE_NONE && state.isGameOver && !net.opponentLost && !sentOver) {
        netSendGameOver(&net);
        sentOver = true;
    }

    uiRender(&state, (net.mode == NET_MODE_NONE) ? NULL : &net);

    const char* endMsg;
    if (net.mode == NET_MODE_NONE) {
        endMsg = state.isGameOver ? "GAME OVER. Press any key to exit." : "Press any key to exit.";
    } else if (net.opponentLost) {
        endMsg = "YOU WIN! Press any key to exit.";
    } else if (!net.connected) {
        endMsg = "Disconnected. Press any key to exit.";
    } else {
        endMsg = "GAME OVER. Press any key to exit.";
    }
    uiShowMessage(endMsg);
    uiWaitKey();

    uiShutdown();
    netClose(&net);
    return 0;
}
