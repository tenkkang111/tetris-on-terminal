#ifndef CORE_H
#define CORE_H

#include "common.h"

// engine.c
void initGame(GameState* state, uint32_t seed);
void initBag(BagSystem* bagSys);
void spawnBlock(GameState* state);
void holdCurrentBlock(GameState* state);

// block.c
bool moveLeft(GameState* state);
bool moveRight(GameState* state);
bool moveDown(GameState* state);            // false = 더 못 내려감 (lock 안함; caller 책임)
void hardDropToBottom(GameState* state);    // 충돌 직전까지 즉시 이동 (lock 안함)
bool rotateCW(GameState* state);
bool rotateCCW(GameState* state);
bool rotate180(GameState* state);
int  ghostDropY(const GameState* state);    // 현재 블록의 ghost 착지 y 반환

// board.c
bool checkCollision(GameState* state, int nextX, int nextY, int nextRot);
int  lockBlock(GameState* state);           // 보드에 박고 라인 클리어. 클리어된 라인 수 반환. spawn/score는 안함
int  checkAndClearLines(GameState* state);
void applyPendingGarbage(GameState* state);
void getPieceCells(BlockType type, int rot, int originX, int originY, int out[4][2]);
TSpinType detectTSpin(const GameState* state); // 직전 액션이 회전이고 type==T일 때만 의미 있음
bool isBoardEmpty(const GameState* state);

#endif
