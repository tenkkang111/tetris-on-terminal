#include "scene.h"
#include "core.h"
#include "ui.h"
#include "network.h"
#include "augment.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GRAVITY_MS      500u
#define TICK_MS          16u
#define LOCK_DELAY_MS   500u
#define MAX_LOCK_RESETS  15

#define DAS_MS           150u
#define ARR_MS            30u
#define HOLD_RELEASE_MS  100u

typedef struct {
    bool     held;
    bool     initialMoveDone;
    uint64_t pressTime;
    uint64_t lastRepeat;
    uint64_t lastInput;
} KeyHold;

typedef bool (*MoveFn)(GameState*);

static uint64_t nowMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void msleep(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static bool handleKeyPress(KeyHold* h, uint64_t now, MoveFn move, GameState* state)
{
    bool moved = false;
    h->lastInput = now;

    if (!h->held) {
        h->held = true;
        h->pressTime = now;
        h->lastRepeat = now;
        h->initialMoveDone = false;
        if (move(state)) {
            moved = true;
            h->initialMoveDone = true;
        }
    }
    return moved;
}

static bool processHeldKey(KeyHold* h, uint64_t now, MoveFn move, GameState* state)
{
    if (!h->held) return false;

    bool moved = false;
    uint64_t elapsed = now - h->pressTime;

    if (elapsed >= DAS_MS) {
        uint64_t sinceLast = now - h->lastRepeat;
        if (sinceLast >= ARR_MS) {
            if (move(state)) moved = true;
            h->lastRepeat = now;
        }
    }
    return moved;
}

static uint8_t comboBonus(uint16_t combo)
{
    static const uint8_t table[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
    if (combo == 0) return 0;
    uint16_t idx = combo - 1;
    if (idx >= sizeof(table) / sizeof(table[0])) return 5;
    return table[idx];
}

static uint8_t computeAttack(int lines, TSpinType tspin, bool perfectClear,
                             bool b2bActive, uint16_t combo,
                             const AugInventory* inv)
{
    int base = 0;
    bool difficult = false;

    if (tspin == TSPIN_FULL) {
        switch (lines) {
            case 1: base = 2; difficult = true; break;
            case 2: base = 4; difficult = true; break;
            case 3: base = 6; difficult = true; break;
        }
        base += inv->tspinAtk;
    } else if (tspin == TSPIN_MINI) {
        switch (lines) {
            case 1: base = 0; difficult = true; break;
            case 2: base = 1; difficult = true; break;
        }
        base += inv->tspinAtk;
    } else {
        switch (lines) {
            case 1: base = 0; break;
            case 2: base = 1; break;
            case 3: base = 2; break;
            case 4: base = 4; difficult = true; break;
        }
        if (lines == 4) base += inv->tetrisAtk;
    }

    int bonus = 0;
    if (difficult && b2bActive) bonus += 1;
    bonus += (int)comboBonus(combo);
    if (combo >= 3) bonus += inv->comboAtk * ((int)combo - 2);
    if (perfectClear && lines > 0) bonus += 10;

    int total = base + bonus;
    if (total < 0) total = 0;
    if (total > 255) total = 255;
    return (uint8_t)total;
}

static uint32_t computeScore(int lines, TSpinType tspin, bool perfectClear,
                             bool b2bBefore, bool stillDifficult, uint16_t comboAfter,
                             const AugInventory* inv)
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
    if (stillDifficult && b2bBefore && lines > 0) s = (s * 3) / 2;
    if (comboAfter >= 2 && lines > 0) s += 50u * (comboAfter - 1);
    if (perfectClear && lines > 0) {
        switch (lines) {
            case 1: s += 800; break;
            case 2: s += 1200; break;
            case 3: s += 1800; break;
            case 4: s += 2000; break;
        }
    }
    s = s * (100u + (uint32_t)inv->scorePct) / 100u;
    return s;
}

static int computeXp(int lines, TSpinType tspin, bool perfectClear, uint16_t comboAfter)
{
    int xp = lines;
    if (lines == 4) xp += 5;
    if (tspin == TSPIN_FULL && lines > 0) xp += 5;
    if (tspin == TSPIN_MINI && lines > 0) xp += 1;
    if (comboAfter >= 2) xp += (int)(comboAfter - 1);
    if (perfectClear && lines > 0) xp += 20;
    return xp;
}

SceneType sceneGame(SceneContext* ctx)
{
    uint32_t seed;
    NetContext* net = NULL;

    if (ctx->netMode != NET_MODE_NONE) {
        net = &ctx->net;
        seed = ctx->seed;
    } else {
        seed = (uint32_t)time(NULL);
    }

    srand(seed);

    GameState state;
    initGame(&state, seed);

    AugInventory augInv;
    augInventoryInit(&augInv);

    LevelState L;
    levelInit(&L);

    if (!uiCheckScreenSize(net != NULL)) {
        if (net) netClose(net);
        return SCENE_LOBBY;
    }

    uint64_t lastGravity = nowMs();
    bool grounded = false;
    uint64_t groundedSince = 0;
    int resetsLeft = MAX_LOCK_RESETS + augInv.lockResetBonus;
    int lastActivePieceTag = -1;

    KeyHold leftH = {0}, rightH = {0}, downH = {0};
    CurrentBlock lastSentBlock = {0};

    bool wasPaused = false;

    while (!state.isGameOver) {
        /* Screen Size Check */
        bool isMulti = (net != NULL);
        if (!uiIsScreenSizeOk(isMulti)) {
            uiDrawPauseOverlay(isMulti);
            wasPaused = true;

            if (net) {
                netPoll(net, &state);
                if (net->opponentLost || !net->connected) break;
            }
            msleep(TICK_MS);
            continue;
        }

        /* Resume from Pause */
        if (wasPaused) {
            wasPaused = false;
            lastGravity = nowMs();
            if (grounded) groundedSince = nowMs();
            leftH.held = rightH.held = downH.held = false;
        }
        bool didLock = false;
        bool actedSuccessfully = false;

        int curTag = (int)state.activeBlock.type * 10000 +
                     (int)state.activeBlock.x * 100 +
                     (int)state.activeBlock.y;
        if (curTag != lastActivePieceTag) {
            grounded = false;
            resetsLeft = MAX_LOCK_RESETS + augInv.lockResetBonus;
            lastActivePieceTag = curTag;
        }

        uint64_t inputNow = nowMs();

        for (;;) {
            UiKey key = uiPollKey();
            if (key == UI_KEY_NONE) break;
            bool ok = false;
            bool doHardDrop = false;
            bool didHold = false;
            switch (key) {
                case UI_KEY_LEFT:
                    ok = handleKeyPress(&leftH, inputNow, moveLeft, &state);
                    rightH.held = false;
                    break;
                case UI_KEY_RIGHT:
                    ok = handleKeyPress(&rightH, inputNow, moveRight, &state);
                    leftH.held = false;
                    break;
                case UI_KEY_SOFT_DROP:
                    ok = handleKeyPress(&downH, inputNow, moveDown, &state);
                    break;
                case UI_KEY_ROTATE_CW:  ok = rotateCW(&state); break;
                case UI_KEY_ROTATE_CCW: ok = rotateCCW(&state); break;
                case UI_KEY_ROTATE_180: ok = rotate180(&state); break;
                case UI_KEY_HARD_DROP:  hardDropToBottom(&state); doHardDrop = true; break;
                case UI_KEY_HOLD: {
                    BlockType prevHold = state.holdBlock;
                    bool prevCanHold = state.canHold;
                    holdCurrentBlock(&state);
                    if (prevCanHold && state.holdBlock != prevHold) didHold = true;
                    break;
                }
                case UI_KEY_QUIT: state.isGameOver = true; break;
                default: break;
            }
            if (ok) actedSuccessfully = true;
            if (didHold && net && net->connected) {
                netSendHold(net, &state);
            }
            if (doHardDrop) { didLock = true; break; }
            if (state.isGameOver) break;
        }

        if (leftH.held  && inputNow - leftH.lastInput  > HOLD_RELEASE_MS) leftH.held  = false;
        if (rightH.held && inputNow - rightH.lastInput > HOLD_RELEASE_MS) rightH.held = false;
        if (downH.held  && inputNow - downH.lastInput  > HOLD_RELEASE_MS) downH.held  = false;

        if (!state.isGameOver && !didLock) {
            if (processHeldKey(&leftH, inputNow, moveLeft, &state)) actedSuccessfully = true;
            if (processHeldKey(&rightH, inputNow, moveRight, &state)) actedSuccessfully = true;
            if (processHeldKey(&downH, inputNow, moveDown, &state)) actedSuccessfully = true;
        }

        uint64_t now = nowMs();
        if (!state.isGameOver && !didLock && now - lastGravity >= GRAVITY_MS) {
            lastGravity = now;
            if (moveDown(&state)) actedSuccessfully = true;
        }

        if (!state.isGameOver && !didLock) {
            bool canDown = !checkCollision(&state,
                                           state.activeBlock.x,
                                           state.activeBlock.y + 1,
                                           state.activeBlock.rotation);
            uint32_t effLockDelay = LOCK_DELAY_MS + (uint32_t)augInv.lockDelayBonus;
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
                if (now - groundedSince >= effLockDelay) didLock = true;
            }
        }

        if (didLock && !state.isGameOver) {
            TSpinType tspin = detectTSpin(&state);
            int lines = lockBlock(&state);
            bool pc = (lines > 0) && isBoardEmpty(&state);

            bool b2bBefore = (state.b2b > 0);
            bool difficult = (lines == 4) || (tspin != TSPIN_NONE && lines > 0);

            uint16_t newCombo = (lines == 0) ? 0 : (state.combo + 1);

            uint8_t attack = computeAttack(lines, tspin, pc, b2bBefore, newCombo, &augInv);
            uint32_t scoreAdd = computeScore(lines, tspin, pc, b2bBefore, difficult, newCombo, &augInv);
            scoreAdd += (uint32_t)augInv.luckyBonusScore;

            state.score      += scoreAdd;
            state.totalLines += (uint32_t)lines;
            state.combo       = newCombo;
            if (lines > 0) state.b2b = difficult ? (uint8_t)(state.b2b + 1) : 0;

            uint8_t toSend = attack;
            if (state.pendingGarbage > 0 && toSend > 0) {
                uint8_t cancel = (toSend < state.pendingGarbage) ? toSend : state.pendingGarbage;
                state.pendingGarbage = (uint8_t)(state.pendingGarbage - cancel);
                toSend = (uint8_t)(toSend - cancel);
            }

            if (net) netSendLock(net, &state, toSend);

            spawnBlock(&state);
            grounded = false;
            resetsLeft = MAX_LOCK_RESETS + augInv.lockResetBonus;
            lastGravity = nowMs();
            leftH.held = rightH.held = downH.held = false;

            int xpRaw = computeXp(lines, tspin, pc, newCombo);
            if (xpRaw > 0) levelGainXp(&L, xpRaw, augInv.xpPct);
        }

        if (net && net->connected && !state.isGameOver) {
            CurrentBlock* cur = &state.activeBlock;
            if (cur->type != lastSentBlock.type ||
                cur->rotation != lastSentBlock.rotation ||
                cur->x != lastSentBlock.x ||
                cur->y != lastSentBlock.y) {
                netSendState(net, cur);
                lastSentBlock = *cur;
            }
        }

        if (net) {
            netPoll(net, &state);

            if (net->incomingGarbageBuf > 0) {
                uint8_t inc = net->incomingGarbageBuf;
                uint8_t absorbed = (augInv.shieldCharges < inc)
                                   ? (uint8_t)augInv.shieldCharges : inc;
                augInv.shieldCharges -= absorbed;
                uint8_t remaining = (uint8_t)(inc - absorbed);
                uint16_t total = (uint16_t)state.pendingGarbage + remaining;
                if (total > 20) total = 20;
                state.pendingGarbage = (uint8_t)total;
                net->incomingGarbageBuf = 0;
            }

            if (net->opponentLost) break;
            if (!net->connected) break;
        }

        while (L.pendingCards > 0 && !state.isGameOver) {
            CardOffer offer;
            cardOfferGenerate(&offer);
            int picked = uiCardSelectModal(&state, net, &augInv, &L, &offer);
            if (picked < 0) { state.isGameOver = true; break; }
            augInventoryAdd(&augInv, offer.offers[picked]);
            L.pendingCards--;
            lastGravity = nowMs();
            grounded = false;
            leftH.held = rightH.held = downH.held = false;
        }

        float lockProgress = -1.0f;
        if (grounded) {
            uint64_t renderNow = nowMs();
            uint32_t effLockDelay = LOCK_DELAY_MS + (uint32_t)augInv.lockDelayBonus;
            uint64_t elapsed = renderNow - groundedSince;
            lockProgress = (float)elapsed / (float)effLockDelay;
            if (lockProgress > 1.0f) lockProgress = 1.0f;
        }

        uiRender(&state, net, &augInv, &L, lockProgress);

        msleep(TICK_MS);
    }

    /* Game Over */
    if (net && net->connected && state.isGameOver && !net->opponentLost) {
        netSendGameOver(net);
    }

    /* Save Result */
    ctx->result.isSinglePlayer = (net == NULL);
    ctx->result.myScore = state.score;
    ctx->result.myLines = state.totalLines;
    ctx->result.myLevel = (uint32_t)L.level;

    if (net) {
        ctx->result.isWin = net->opponentLost;
        ctx->result.isDisconnect = !net->connected && !net->opponentLost;
        ctx->result.oppScore = net->opponentScore;
        ctx->result.oppLines = net->opponentTotalLines;
        netClose(net);
    } else {
        ctx->result.isWin = false;
        ctx->result.isDisconnect = false;
        ctx->result.oppScore = 0;
        ctx->result.oppLines = 0;
    }

    return SCENE_RESULT;
}
