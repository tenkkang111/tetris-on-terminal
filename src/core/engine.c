#include <string.h>
#include <stdlib.h>
#include "core.h"


#define SPAWN_X   4  // 블럭 초기 X 값
#define SPAWN_Y   0  // 블럭 초기 Y 값
#define BAG_SIZE 7  // 7-Bag 시스템 크기

// LCG 상수 (POSIX rand 기준)
#define LCG_A 1103515245
#define LCG_C 12345
#define LCG_M 2147483648 // 2^31

// 내부 utility 함수

/**
 * @brief 독립 난수 생성기 (LCG 알고리즘)
 */
static uint32_t bagRand(uint32_t* seed_state) {
    *seed_state = (*seed_state * LCG_A + LCG_C) % LCG_M;
    return *seed_state;
}

/**
 * @brief BlockType 값 교환
 */
static inline void swapBlockType(BlockType* a, BlockType* b)
{
    BlockType tmp = *a;
    *a = *b;
    *b = tmp;
}

/**
 * @brief 특정 배열을 7-Bag 규칙에 맞게 채우고 섞음
 * 내부 헬퍼 함수
 */
static void fillAndShuffleBag(BlockType* bagArray, uint32_t* seed_state) {
    for (int i = 0; i < BAG_SIZE; i++) {
        bagArray[i] = (BlockType)(I + i);
    }

    for (int i = BAG_SIZE - 1; i > 0; i--) {
        int j = bagRand(seed_state) % (i + 1);
        swapBlockType(&bagArray[i], &bagArray[j]);
    }
}

// 공개 함수

/**
 * @brief 게임 상태 초기화
 */

void initGame(GameState* state, uint32_t seed)
{
    memset(state->board, EMPTY, sizeof(state->board));
 
    state->score          = 0;
    state->pendingGarbage = 0;
    state->isGameOver     = false;

    state->bagState.currentSeed = seed;
 
    initBag(&state->bagState);
    spawnBlock(state);
}

/**
 * @brief 7-Bag 배열에 I~Z 블록을 채우고, 셔플
 *
 * 셔플 후 currentIndex를 0으로 설정
 */
void initBag(BagSystem* bagSys){
    fillAndShuffleBag(bagSys->bag, &bagSys->currentSeed);
    fillAndShuffleBag(bagSys->nextBag, &bagSys->currentSeed);
    bagSys->currentIndex = 0;
}


/**
 * @brief 가방에서 블록을 꺼내 activeBlock으로 설정
 */
void spawnBlock(GameState* state){
    BagSystem* bag = &state->bagState;

    if (bag->currentIndex >= BAG_SIZE) {
        for (int i = 0; i < BAG_SIZE; i++) {
            bag->bag[i] = bag->nextBag[i];
        }

        fillAndShuffleBag(bag->nextBag, &bag->currentSeed);

        bag->currentIndex = 0;
    }

    if (state->pendingGarbage > 0) {
        applyPendingGarbage(state);
    }

    BlockType nextType = bag->bag[bag->currentIndex++];

    state->activeBlock.type = nextType;
    state->activeBlock.rotation = 0;
    state->activeBlock.x = (int8_t)SPAWN_X;
    state->activeBlock.y = (int8_t)SPAWN_Y;
    
    // 스폰 충돌 여부 체크
    if (checkCollision(state, SPAWN_X, SPAWN_Y, 0)) {
        state->isGameOver = true;
    }
}