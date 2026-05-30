#include "core.h"

#define ROTATION_COUNT  4

/**
 * SRS Wall Kick 오프셋
 *   - JLSTZ 블록용과 I 블록용이 상이
 *   kicksJLSTZ[현재_회전][킥_인덱스] = {dx, dy}
 *
 * dy는 화면 아래 방향이 양수인 좌표 기준
 */
static const int kicksJLSTZ[4][5][2] = {
    /* 0 → 1 */ {{ 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}},
    /* 1 → 2 */ {{ 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}},
    /* 2 → 3 */ {{ 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}},
    /* 3 → 0 */ {{ 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}},
};

/**
 * I 블록 전용 Wall Kick 오프셋 테이블
 *
 * kicksI[현재_회전][킥_인덱스] = {dx, dy}
 */
static const int kicksI[4][5][2] = {
    /* 0 → 1 */ {{ 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}},
    /* 1 → 2 */ {{ 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}},
    /* 2 → 3 */ {{ 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}},
    /* 3 → 0 */ {{ 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}},
};

// 공개 함수 

/**
 * @brief 블록을 좌로 1칸 이동 시도
 * 
 * @param state 현재 GameState 포인터
 * @return 이동 성공시 True, 충돌 등 사유로 불가 시 false
 */
bool moveLeft(GameState* state){
    CurrentBlock* b  = &state->activeBlock;
    int           nx = b->x - 1;

    if (checkCollision(state, nx, b->y, b->rotation)) {
        return false;
    }

    b->x = (int8_t)nx;
    return true;
}

/**
 * @brief 블록을 우로 1칸 이동 시도
 * 
 * @param state 현재 GameState 포인터
 * @return 이동 성공시 True, 충돌 등 사유로 불가 시 false
 */
bool moveRight(GameState* state){
    CurrentBlock* b  = &state->activeBlock;
    int           nx = b->x + 1;

    if (checkCollision(state, nx, b->y, b->rotation)) {
        return false;
    }

    b->x = (int8_t)nx;
    return true;
}

/**
 * @brief 블록을 아래로 1칸 이동 시도
 * 
 * 충돌 발생시 이동을 취소하고 lockBlock() 호출
 * 
 * @param state 현재 GameState 포인터
 * @return 이동 성공시 True, 충돌 등 사유로 불가 시 false
 */
bool moveDown(GameState* state){
    CurrentBlock* b  = &state->activeBlock;
    int           ny = b->y + 1;

    if (checkCollision(state, b->x, ny, b->rotation)) {
        lockBlock(state);
        return false;
    }

    b->y = (int8_t)ny;
    return true;
}

/**
 * @brief 블록을 충돌 직전 위치까지 즉시 낙하
 * 
 * CheckCollision이 True를 반환할 때까지 y 좌표 증가
 * lockblock 호출
 * 
 * @param state 현재 GameState 포인터
 */
void hardDrop(GameState* state){
    CurrentBlock* b = &state->activeBlock;
    int targetY = b->y;

    // 충돌하기 전까지의 최대 Y값을 찾음
    while (!checkCollision(state, b->x, targetY + 1, b->rotation)) {
        targetY++;
    }

    // 한 번에 위치 업데이트 후 블록 고정
    b->y = (int8_t)targetY;
    lockBlock(state);
}
    
/**
 * @brief 블록 회전 시도
 * 
 * 0 블럭은 생략
 * 충돌 발생 시 SRS Wall Kick 오프셋 5개를 순차적 시도
 * 모든 시도가 실패 시, 회전 취소 및 false 반환
 * 
 * @param state 현재 GameState 포인터
 * @return 회전 성공시 True, 그 외 false
 */
bool rotateBlock(GameState* state){
    CurrentBlock* b = &state->activeBlock;
    int           newRotation = (b->rotation + 1) % ROTATION_COUNT;

    if (b->type == O) {
        return true;
    }

    const int (*kicks)[2] = (b->type == I)
                            ? kicksI[b->rotation]
                            : kicksJLSTZ[b->rotation];

    for (int i = 0; i < 5; i++) {
        int kx = b->x + kicks[i][0];
        int ky = b->y + kicks[i][1];

        if (!checkCollision(state, kx, ky, newRotation)) {
            b->x = (int8_t)kx;
            b->y = (int8_t)ky;
            b->rotation = (uint8_t)newRotation;
            return true;
        }
    }
    
    return false;
}
