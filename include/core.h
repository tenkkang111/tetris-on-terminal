#ifndef CORE_H
#define CORE_H

#include "common.h"

// engine.c
void initGame(GameState* state);
void initBag(BagSystem* bagSys);
void spawnBlock(GameState* state);

// block.c
bool moveLeft(GameState* state);
bool moveRight(GameState* state);
bool moveDown(GameState* state);
void hardDrop(GameState* state);
bool rotateBlock(GameState* state);

// board.c
bool checkCollision(GameState* state, int nextX, int nextY, int nextRot);
void lockBlock(GameState* state);
int checkAndClearLines(GameState* state);
void applyPendingGarbage(GameState* state);

#endif