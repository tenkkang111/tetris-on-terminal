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
bool moveDown(GameState* state);
void hardDrop(GameState* state);
bool rotateBlock(GameState* state);

// board.c
bool checkCollision(GameState* state, int nextX, int nextY, int nextRot);
void lockBlock(GameState* state); // network 이벤트 타이밍
int checkAndClearLines(GameState* state);
void applyPendingGarbage(GameState* state);
void getPieceCells(BlockType type, int rot, int originX, int originY, int out[4][2]);

#endif