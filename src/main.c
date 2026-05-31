#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "core.h"
#include "network.h"
#include "ui.h"

#define GRAVITY_MS 500u
#define TICK_MS     16u
#define DEFAULT_PORT 5555u

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

/* score 증가량 → 클리어된 라인 수 (board.c의 lineScore 테이블 역산) */
static int linesFromScoreDelta(uint32_t delta)
{
    switch (delta) {
        case 100: return 1;
        case 300: return 2;
        case 500: return 3;
        case 800: return 4;
        default:  return 0;
    }
}

/* 라인 수 → 상대에게 보낼 garbage 줄 수 (Tetris 표준 공격 테이블) */
static uint8_t garbageForLines(int lines)
{
    switch (lines) {
        case 2: return 1;
        case 3: return 2;
        case 4: return 4;
        default: return 0;
    }
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

    /* 인자 파싱 */
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
        if (netHost(&net, port) != 0) {
            fprintf(stderr, "Failed to host on port %u.\n", port);
            return 1;
        }
        if (netExchangeSeed(&net) != 0) {
            fprintf(stderr, "Seed exchange failed.\n");
            netClose(&net);
            return 1;
        }
        seed = net.seed;
        fprintf(stderr, "Opponent connected. Seed=%u\n", seed);
    } else if (mode == NET_MODE_CLIENT) {
        fprintf(stderr, "Connecting to %s:%u...\n", host, port);
        if (netJoin(&net, host, port) != 0) {
            fprintf(stderr, "Failed to connect.\n");
            return 1;
        }
        if (netExchangeSeed(&net) != 0) {
            fprintf(stderr, "Seed exchange failed.\n");
            netClose(&net);
            return 1;
        }
        seed = net.seed;
        fprintf(stderr, "Connected. Seed=%u\n", seed);
    } else {
        seed = (uint32_t)time(NULL);
    }

    srand(seed); /* applyPendingGarbage 등의 rand() 용 (싱글: 충분, 멀티: TODO 동기화) */

    GameState state;
    initGame(&state, seed);

    uiInit();

    uint64_t lastGravity = nowMs();
    bool sentGameOver = false;

    while (!state.isGameOver) {
        uint32_t prevScore = state.score;
        bool didLock = false;
        bool didHardDrop = false;

        /* 입력 (한 틱에 여러 키 소진) */
        for (;;) {
            UiKey key = uiPollKey();
            if (key == UI_KEY_NONE) break;
            switch (key) {
                case UI_KEY_LEFT:      moveLeft(&state); break;
                case UI_KEY_RIGHT:     moveRight(&state); break;
                case UI_KEY_DOWN:
                    if (!moveDown(&state)) didLock = true;
                    break;
                case UI_KEY_ROTATE:    rotateBlock(&state); break;
                case UI_KEY_HARD_DROP:
                    hardDrop(&state);
                    didLock = true;
                    didHardDrop = true;
                    break;
                case UI_KEY_HOLD:      holdCurrentBlock(&state); break;
                case UI_KEY_QUIT:
                    state.isGameOver = true;
                    break;
                default: break;
            }
            if (state.isGameOver) break;
            /* hardDrop 후 같은 틱에 다른 키가 같이 들어오면 다음 블록에 적용됨 — 의도된 동작 */
            (void)didHardDrop;
        }

        /* gravity */
        uint64_t now = nowMs();
        if (!state.isGameOver && now - lastGravity >= GRAVITY_MS) {
            lastGravity = now;
            if (!moveDown(&state)) didLock = true;
        }

        /* lock 발생 시 멀티에 동기화 */
        if (didLock && net.mode != NET_MODE_NONE) {
            int lines = linesFromScoreDelta(state.score - prevScore);
            uint8_t garbage = garbageForLines(lines);
            netSendLock(&net, &state, garbage);
        }

        /* 활성 블록 위치 매 틱 송신 (상대 화면에 실시간 표시) */
        if (net.mode != NET_MODE_NONE && net.connected && !state.isGameOver) {
            netSendState(&net, &state.activeBlock);
        }

        /* 상대 메시지 폴링 */
        if (net.mode != NET_MODE_NONE) {
            netPoll(&net, &state);
            if (net.opponentLost) {
                /* 상대 게임오버 = 승리. 루프 종료 */
                break;
            }
            if (!net.connected) {
                /* 상대 연결 끊김 */
                break;
            }
        }

        uiRender(&state, (net.mode == NET_MODE_NONE) ? NULL : &net);

        msleep(TICK_MS);
    }

    /* 종료 처리 */
    if (net.mode != NET_MODE_NONE && state.isGameOver && !net.opponentLost && !sentGameOver) {
        netSendGameOver(&net);
        sentGameOver = true;
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
