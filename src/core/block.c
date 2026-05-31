#include "core.h"

#define ROTATION_COUNT 4

/* SRS Wall Kick — JLSTZ (CW) */
static const int kicksJLSTZ_CW[4][5][2] = {
    /* 0 -> 1 */ {{ 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}},
    /* 1 -> 2 */ {{ 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}},
    /* 2 -> 3 */ {{ 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}},
    /* 3 -> 0 */ {{ 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}},
};

/* SRS Wall Kick — JLSTZ (CCW) */
static const int kicksJLSTZ_CCW[4][5][2] = {
    /* 0 -> 3 */ {{ 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}},
    /* 1 -> 0 */ {{ 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}},
    /* 2 -> 1 */ {{ 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}},
    /* 3 -> 2 */ {{ 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}},
};

/* SRS Wall Kick — I (CW) */
static const int kicksI_CW[4][5][2] = {
    /* 0 -> 1 */ {{ 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}},
    /* 1 -> 2 */ {{ 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}},
    /* 2 -> 3 */ {{ 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}},
    /* 3 -> 0 */ {{ 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}},
};

/* SRS Wall Kick — I (CCW) */
static const int kicksI_CCW[4][5][2] = {
    /* 0 -> 3 */ {{ 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}},
    /* 1 -> 0 */ {{ 0, 0}, { 2, 0}, {-1, 0}, { 2, 1}, {-1,-2}},
    /* 2 -> 1 */ {{ 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}},
    /* 3 -> 2 */ {{ 0, 0}, {-2, 0}, { 1, 0}, {-2,-1}, { 1, 2}},
};

/* 180° kick (TETR.IO 단순화 버전: 본 위치 + 좌우 + 위 시도) */
static const int kicks180[6][2] = {
    { 0, 0}, { 1, 0}, {-1, 0}, { 0,-1}, { 2, 0}, {-2, 0},
};

bool moveLeft(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    if (checkCollision(state, b->x - 1, b->y, b->rotation)) return false;
    b->x -= 1;
    state->lastActionRotation = false;
    return true;
}

bool moveRight(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    if (checkCollision(state, b->x + 1, b->y, b->rotation)) return false;
    b->x += 1;
    state->lastActionRotation = false;
    return true;
}

bool moveDown(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    if (checkCollision(state, b->x, b->y + 1, b->rotation)) return false;
    b->y += 1;
    state->lastActionRotation = false;
    return true;
}

void hardDropToBottom(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    while (!checkCollision(state, b->x, b->y + 1, b->rotation)) {
        b->y += 1;
    }
    state->lastActionRotation = false;
}

int ghostDropY(const GameState* state)
{
    /* checkCollision은 state 비-const이지만 보드 변경은 안 함 — 캐스팅 */
    GameState* mut = (GameState*)state;
    const CurrentBlock* b = &state->activeBlock;
    int y = b->y;
    while (!checkCollision(mut, b->x, y + 1, b->rotation)) y++;
    return y;
}

static bool tryRotate(GameState* state, int newRot, const int (*kicks)[2], int kickCount)
{
    CurrentBlock* b = &state->activeBlock;
    for (int i = 0; i < kickCount; i++) {
        int kx = b->x + kicks[i][0];
        int ky = b->y + kicks[i][1];
        if (!checkCollision(state, kx, ky, newRot)) {
            b->x = (int8_t)kx;
            b->y = (int8_t)ky;
            b->rotation = (uint8_t)newRot;
            state->lastActionRotation = true;
            return true;
        }
    }
    return false;
}

bool rotateCW(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    if (b->type == O) { state->lastActionRotation = true; return true; }
    int newRot = (b->rotation + 1) % ROTATION_COUNT;
    const int (*kicks)[2] = (b->type == I)
                            ? kicksI_CW[b->rotation]
                            : kicksJLSTZ_CW[b->rotation];
    return tryRotate(state, newRot, kicks, 5);
}

bool rotateCCW(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    if (b->type == O) { state->lastActionRotation = true; return true; }
    int newRot = (b->rotation + 3) % ROTATION_COUNT;
    const int (*kicks)[2] = (b->type == I)
                            ? kicksI_CCW[b->rotation]
                            : kicksJLSTZ_CCW[b->rotation];
    return tryRotate(state, newRot, kicks, 5);
}

bool rotate180(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    if (b->type == O) { state->lastActionRotation = true; return true; }
    int newRot = (b->rotation + 2) % ROTATION_COUNT;
    return tryRotate(state, newRot, kicks180, 6);
}
