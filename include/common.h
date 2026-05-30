#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

// 블럭 타입 정의
typedef enum {
    EMPTY = 0, I, J, L, O, S, T, Z, GARBAGE
} BlockType;

// 현재 블럭 정보
typedef struct {
    int8_t x, y;
    BlockType type;
    uint8_t rotation;
} CurrentBlock;

// 7-Bag 시스템 관리 구조체
typedef struct {
    BlockType bag[7];
    uint8_t currentIndex;
    uint32_t currentSeed;
    BlockType nextBag[7];
} BagSystem;

// 게임 상태 구조체
typedef struct {
    uint8_t board[20][10];   // 20*10 게임 보드
    CurrentBlock activeBlock;// 활성 블럭 정보
    BagSystem bagState;      // 7-Bag 정보
    BlockType holdBlock;     // 홀드 블럭 정보
    bool canHold;            // 홀드 가능 여부
    uint32_t score;          // 점수
    uint8_t pendingGarbage;  // 대기 가비지 라인 수(Multi 용도)
    bool isGameOver;         // 게임 종료 여부
} GameState;

#endif