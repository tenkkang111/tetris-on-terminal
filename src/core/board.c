#include <stdlib.h>
#include "core.h"

#define BOARD_ROWS   20
#define BOARD_COLS   10
#define PIECE_CELLS   4  // 모든 블럭 4칸 고정

/**
 * SRS 회전 형태 테이블
 *
 * shapes[BlockType-1][rotation][cell][{row_offset, col_offset}]
 *
 * 좌표계: row 증가 = 아래, col 증가 = 오른쪽
 * 블록 원점(x, y)에서의 상대 오프셋으로 4개 셀 위치를 표현합니다.
 */

 /* I=0, J=1, L=2, O=3, S=4, T=5, Z=6 */
static const int8_t shapes[7][4][4][2] = {
    // I:0
    {
        {{ 1, 0}, { 1, 1}, { 1, 2}, { 1, 3}},
        {{ 0, 2}, { 1, 2}, { 2, 2}, { 3, 2}},
        {{ 2, 0}, { 2, 1}, { 2, 2}, { 2, 3}},
        {{ 0, 1}, { 1, 1}, { 2, 1}, { 3, 1}},
    },

    // J:1
    {
        {{ 0, 0}, { 1, 0}, { 1, 1}, { 1, 2}},
        {{ 0, 1}, { 0, 2}, { 1, 1}, { 2, 1}},
        {{ 1, 0}, { 1, 1}, { 1, 2}, { 2, 2}},
        {{ 0, 1}, { 1, 1}, { 2, 0}, { 2, 1}},
    },

    // L:2
    {
        {{ 0, 2}, { 1, 0}, { 1, 1}, { 1, 2}},
        {{ 0, 1}, { 1, 1}, { 2, 1}, { 2, 2}},
        {{ 1, 0}, { 1, 1}, { 1, 2}, { 2, 0}},
        {{ 0, 0}, { 0, 1}, { 1, 1}, { 2, 1}},
    },

    // O:3
    {
        {{ 0, 0}, { 0, 1}, { 1, 0}, { 1, 1}},
        {{ 0, 0}, { 0, 1}, { 1, 0}, { 1, 1}},
        {{ 0, 0}, { 0, 1}, { 1, 0}, { 1, 1}},
        {{ 0, 0}, { 0, 1}, { 1, 0}, { 1, 1}},
    },

    // S:4
    {
        {{ 0, 1}, { 0, 2}, { 1, 0}, { 1, 1}},
        {{ 0, 1}, { 1, 1}, { 1, 2}, { 2, 2}},
        {{ 1, 1}, { 1, 2}, { 2, 0}, { 2, 1}},
        {{ 0, 0}, { 1, 0}, { 1, 1}, { 2, 1}},
    },

    // T:5
    {
        {{ 0, 1}, { 1, 0}, { 1, 1}, { 1, 2}},
        {{ 0, 1}, { 1, 1}, { 1, 2}, { 2, 1}},
        {{ 1, 0}, { 1, 1}, { 1, 2}, { 2, 1}},
        {{ 0, 1}, { 1, 0}, { 1, 1}, { 2, 1}},
    },

    // Z:6
    {
        {{ 0, 0}, { 0, 1}, { 1, 1}, { 1, 2}},
        {{ 0, 2}, { 1, 1}, { 1, 2}, { 2, 1}},
        {{ 1, 0}, { 1, 1}, { 2, 1}, { 2, 2}},
        {{ 0, 1}, { 1, 0}, { 1, 1}, { 2, 0}},
    },
};

// 내부 utility 함수

/**
 * @brief BlockType → shapes 배열 인덱스 변환 (I=0 … Z=6).
 *        EMPTY, GARBAGE는 호출 않음
 */
static inline int typeToIndex(BlockType type)
{
    return (int)type - 1;
}

/**
 * @brief 특정 블록의 회전 상태에 해당하는 절대 보드 좌표를 반환(4개 셀)
 *
 * @param type   블록 종류
 * @param rot    회전 상태 (0~3)
 * @param originX 블록 원점의 열(x)
 * @param originY 블록 원점의 행(y)
 * @param out    결과를[4][2] 배열
 */
void getPieceCells(BlockType type, int rot,
                   int originX, int originY,
                   int out[4][2])
{
    const int8_t (*shape)[2] = shapes[typeToIndex(type)][rot];
    for (int i = 0; i < PIECE_CELLS; i++) {
        out[i][0] = originY + shape[i][0];
        out[i][1] = originX + shape[i][1];
    }
}

// 공개 함수


/**
 * @brief 목표 좌표/회전 상태에서 충돌 여부를 판
 *
 *   1. 왼쪽 오른쪽 벽 이탈    (col < 0 || col >= BOARD_COLS)
 *   2. 바닥 이탈              (row >= BOARD_ROWS)
 *   3. 이미 채워진 셀과 겹침  (board[row][col] != EMPTY)
 *   시에 충돌로 간주.
 *
 * 천장 위(row < 0)는 스폰 직후 위치이므로 허용
 *
 * @return 충돌 시 true, 없으면 false
 */
bool checkCollision(GameState* state, int nextX, int nextY, int nextRot)
{
    int cells[4][2];
    getPieceCells(state->activeBlock.type, nextRot, nextX, nextY, cells);
 
    for (int i = 0; i < PIECE_CELLS; i++) {
        int row = cells[i][0];
        int col = cells[i][1];
 
        /* 좌우 벽 및 바닥 */
        if (col < 0 || col >= BOARD_COLS || row >= BOARD_ROWS) {
            return true;
        }
 
        /* 천장 위는 허용 (스폰 위치) */
        if (row < 0) {
            continue;
        }
 
        /* 기존 블록과 겹침 */
        if (state->board[row][col] != EMPTY) {
            return true;
        }
    }
 
    return false;
}

/**
 * @brief activeBlock을 board 2차원 배열에 고정 + 라인 클리어.
 *
 *   - 점수/스폰/가비지 큐 처리는 호출자(main.c)가 담당.
 *   - 멀티플레이 시 본 함수 직후가 네트워크 송신 타이밍.
 *
 * @return 클리어된 라인 수 (0~4)
 */
int lockBlock(GameState* state)
{
    CurrentBlock* b = &state->activeBlock;
    int cells[4][2];
    getPieceCells(b->type, b->rotation, b->x, b->y, cells);

    for (int i = 0; i < PIECE_CELLS; i++) {
        int row = cells[i][0];
        int col = cells[i][1];

        if (row >= 0 && row < BOARD_ROWS &&
            col >= 0 && col < BOARD_COLS) {
            state->board[row][col] = (uint8_t)b->type;
        }
    }

    return checkAndClearLines(state);
}

/**
 * @brief T-Spin 판정 (3-corner rule).
 *        호출 직전 액션이 회전 성공 + activeBlock.type == T 일 때만 의미 있음.
 *
 *   - 4개 대각 코너 중 3개 이상 막힘 → T-spin 후보
 *   - 그 중 'front' 두 코너 모두 막힘 → TSPIN_FULL
 *   - 아니면 TSPIN_MINI
 *   - 코너 2개 이하 막힘 → TSPIN_NONE
 *
 * T의 중심은 활성 블록 원점 (x,y) 에서 (1,1) 오프셋.
 */
TSpinType detectTSpin(const GameState* state)
{
    const CurrentBlock* b = &state->activeBlock;
    if (b->type != T || !state->lastActionRotation) return TSPIN_NONE;

    int cx = b->x + 1;
    int cy = b->y + 1;

    /* (dr, dc) 4 대각 */
    const int diag[4][2] = {
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };

    int filled = 0;
    bool occ[4];
    for (int i = 0; i < 4; i++) {
        int r = cy + diag[i][0];
        int c = cx + diag[i][1];
        bool isFilled;
        if (r < 0 || r >= BOARD_ROWS || c < 0 || c >= BOARD_COLS) {
            isFilled = true;        /* 벽/바닥은 막힘으로 취급 */
        } else {
            isFilled = (state->board[r][c] != EMPTY);
        }
        occ[i] = isFilled;
        if (isFilled) filled++;
    }

    if (filled < 3) return TSPIN_NONE;

    /* rotation별 'front' 두 코너 인덱스 (diag 배열 기준).
     *   diag[0]=TL, diag[1]=TR, diag[2]=BL, diag[3]=BR
     *   rot 0 = stem up    → front = TL, TR
     *   rot 1 = stem right → front = TR, BR
     *   rot 2 = stem down  → front = BL, BR
     *   rot 3 = stem left  → front = TL, BL
     */
    int frontA, frontB;
    switch (b->rotation) {
        case 0: frontA = 0; frontB = 1; break;
        case 1: frontA = 1; frontB = 3; break;
        case 2: frontA = 2; frontB = 3; break;
        case 3: frontA = 0; frontB = 2; break;
        default: return TSPIN_NONE;
    }

    if (occ[frontA] && occ[frontB]) return TSPIN_FULL;
    return TSPIN_MINI;
}

/**
 * @brief 보드가 완전히 비어 있는지 (Perfect Clear 판정용).
 */
bool isBoardEmpty(const GameState* state)
{
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            if (state->board[r][c] != EMPTY) return false;
        }
    }
    return true;
}

/**
 * @brief 라인 클리어, 블럭 이동 등 처리
 *
 * @return 삭제된 line 수
 */
int checkAndClearLines(GameState* state)
{
    int cleared = 0;

    int row = BOARD_ROWS - 1;
    while (row >= 0) {
        bool full = true;
        for (int col = 0; col < BOARD_COLS; col++) {
            if (state->board[row][col] == EMPTY) {
                full = false;
                break;
            }
        }
        if (!full) {
            row--;
            continue;
        }

        for (int r = row; r > 0; r--) {
            for (int c = 0; c < BOARD_COLS; c++) {
                state->board[r][c] = state->board[r - 1][c];
            }
        }
        for (int col = 0; col < BOARD_COLS; col++) {
            state->board[0][col] = EMPTY;
        }
        cleared++;
    }

    if (cleared > 4) cleared = 4; // 혹시 모르니 안전장치

    return cleared;
}

/**
 * @brief pendingGarbage 줄을 맵 하단에 삽입합니다.
 * 
 * 멀티플레이 전용
 * 
 * 전체 블록을 pendingGarbage 줄 수만큼 위로 밀어올림
 *  - 천장 위로 밀려나면 게임 오버
 * 맵 하단에 pendingGarbage 줄 수만큼 GARBAGE 블록
 * pendingGarbage는 0으로 초기화
 * 
 * spawnBlock()에서 호출
 */
void applyPendingGarbage(GameState* state){
    uint8_t lines = state->pendingGarbage;
    if (lines == 0) return;


    // 천장 이탈 체크
    for (int r = 0; r < (int)lines && r < BOARD_ROWS; r++) {
        for (int col = 0; col < BOARD_COLS; col++) {
            if (state->board[r][col] != EMPTY) {
                state->isGameOver     = true;
                state->pendingGarbage = 0;
                return;
            }
        }
    }

    // 보드 위로 이동
    for (int r = 0; r < BOARD_ROWS; r++) {
        int srcRow = r + (int)lines;
 
        if (srcRow < BOARD_ROWS) {
            for (int col = 0; col < BOARD_COLS; col++) {
                state->board[r][col] = state->board[srcRow][col];
            }
        } else {
            for (int col = 0; col < BOARD_COLS; col++) {
                state->board[r][col] = EMPTY;
            }
        }
    }

    /* 맨 아래 lines 줄에 구멍 1개 뚫린 GARBAGE 줄 삽입
     *    같은 구멍 위치를 한 묶음에 유지 
     */

    //TODO: rand 시드 동기화
    int holeCol = rand() % BOARD_COLS;
 
    for (int i = 0; i < (int)lines; i++) {
        int row = BOARD_ROWS - (int)lines + i;
        for (int col = 0; col < BOARD_COLS; col++) {
            state->board[row][col] = (col == holeCol) ? EMPTY : GARBAGE;
        }
    }

    state->pendingGarbage = 0;
}