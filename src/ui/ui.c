#include "ui.h"
#include "core.h"

#include <ncurses.h>
#include <string.h>
#include <stdio.h>

#define BOARD_W 10
#define BOARD_H 20

/* 한 셀당 화면 폭 2칸 (블록 1칸 = "[]") */
#define CELL_W  2

static bool gColors = false;

static int colorForType(BlockType t)
{
    /* I=1 .. Z=7, GARBAGE=8 — color pair number == enum value */
    return (int)t;
}

void uiInit(void)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(I,       COLOR_CYAN,    -1);
        init_pair(J,       COLOR_BLUE,    -1);
        init_pair(L,       COLOR_YELLOW,  -1);
        init_pair(O,       COLOR_YELLOW,  -1);
        init_pair(S,       COLOR_GREEN,   -1);
        init_pair(T,       COLOR_MAGENTA, -1);
        init_pair(Z,       COLOR_RED,     -1);
        init_pair(GARBAGE, COLOR_WHITE,   -1);
        gColors = true;
    }
}

void uiShutdown(void)
{
    endwin();
}

UiKey uiPollKey(void)
{
    int ch = getch();
    switch (ch) {
        case ERR:       return UI_KEY_NONE;
        case KEY_LEFT:  return UI_KEY_LEFT;
        case KEY_RIGHT: return UI_KEY_RIGHT;
        case KEY_DOWN:  return UI_KEY_DOWN;
        case KEY_UP:    return UI_KEY_ROTATE;
        case ' ':       return UI_KEY_HARD_DROP;
        case 'c': case 'C': return UI_KEY_HOLD;
        case 'q': case 'Q': return UI_KEY_QUIT;
        default:        return UI_KEY_NONE;
    }
}

/* 한 셀 그리기 (스크린 좌표 row, col 기준; col은 CELL_W만큼 차지) */
static void drawCell(int row, int col, uint8_t type)
{
    if (type == EMPTY) {
        mvaddstr(row, col, " .");
        return;
    }
    int cp = colorForType((BlockType)type);
    if (gColors) attron(COLOR_PAIR(cp) | A_BOLD);
    if (type == GARBAGE) mvaddstr(row, col, "##");
    else                  mvaddstr(row, col, "[]");
    if (gColors) attroff(COLOR_PAIR(cp) | A_BOLD);
}

/* 4x4 미니 박스 안에 블록 모양 출력 (HOLD/NEXT 미리보기) */
static void drawMiniPiece(int top, int left, BlockType type)
{
    /* 박스 내부 4행 x 4열을 빈 칸으로 비움 */
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            mvaddstr(top + r, left + c * CELL_W, "  ");
        }
    }
    if (type == EMPTY) return;

    int cells[4][2];
    getPieceCells(type, 0, 0, 0, cells);

    /* shapes 테이블은 0~3 행/열을 사용. 그대로 매핑. */
    for (int i = 0; i < 4; i++) {
        int r = cells[i][0];
        int c = cells[i][1];
        if (r >= 0 && r < 4 && c >= 0 && c < 4) {
            drawCell(top + r, left + c * CELL_W, (uint8_t)type);
        }
    }
}

/* 보드 외곽 박스 그리기. 내부 영역 좌상단(스크린 좌표) = (top+1, left+1) */
static void drawBoardFrame(int top, int left)
{
    int width = BOARD_W * CELL_W;
    mvaddch(top, left, ACS_ULCORNER);
    for (int i = 0; i < width; i++) mvaddch(top, left + 1 + i, ACS_HLINE);
    mvaddch(top, left + 1 + width, ACS_URCORNER);

    for (int r = 0; r < BOARD_H; r++) {
        mvaddch(top + 1 + r, left, ACS_VLINE);
        mvaddch(top + 1 + r, left + 1 + width, ACS_VLINE);
    }

    mvaddch(top + 1 + BOARD_H, left, ACS_LLCORNER);
    for (int i = 0; i < width; i++) mvaddch(top + 1 + BOARD_H, left + 1 + i, ACS_HLINE);
    mvaddch(top + 1 + BOARD_H, left + 1 + width, ACS_LRCORNER);
}

/* 미니 박스(4x4 셀) 외곽선 */
static void drawMiniFrame(int top, int left, const char* label)
{
    int width = 4 * CELL_W;
    if (label) mvprintw(top - 1, left, "%s", label);
    mvaddch(top, left - 1, ACS_ULCORNER);
    for (int i = 0; i < width; i++) mvaddch(top, left + i, ACS_HLINE);
    mvaddch(top, left + width, ACS_URCORNER);
    for (int r = 0; r < 4; r++) {
        mvaddch(top + 1 + r, left - 1, ACS_VLINE);
        mvaddch(top + 1 + r, left + width, ACS_VLINE);
    }
    mvaddch(top + 1 + 4, left - 1, ACS_LLCORNER);
    for (int i = 0; i < width; i++) mvaddch(top + 1 + 4, left + i, ACS_HLINE);
    mvaddch(top + 1 + 4, left + width, ACS_LRCORNER);
}

/* 한 플레이어 패널 렌더링.
 * leftCol: 패널의 왼쪽 시작 화면 col
 * activeOverlay: NULL이면 오버레이 생략 (상대 화면용)
 */
static void drawPlayerPanel(int topRow, int leftCol, const char* title,
                            const uint8_t board[20][10],
                            const CurrentBlock* activeOverlay,
                            BlockType holdBlock,
                            const BagSystem* bag,
                            uint32_t score,
                            uint8_t pendingGarbage)
{
    mvprintw(topRow, leftCol, "%s", title);

    /* HOLD 박스: 왼쪽에 4x4 미니 박스 */
    int holdTop  = topRow + 3;
    int holdLeft = leftCol + 1;
    drawMiniFrame(holdTop, holdLeft, "HOLD");
    drawMiniPiece(holdTop + 1, holdLeft, holdBlock);

    /* 보드 박스: HOLD 박스 우측, 약간 공백 */
    int boardTop  = topRow + 2;
    int boardLeft = leftCol + 1 + 4 * CELL_W + 4;
    drawBoardFrame(boardTop, boardLeft);

    /* 보드 셀 */
    for (int r = 0; r < BOARD_H; r++) {
        for (int c = 0; c < BOARD_W; c++) {
            drawCell(boardTop + 1 + r, boardLeft + 1 + c * CELL_W, board[r][c]);
        }
    }

    /* 활성 블록 오버레이 */
    if (activeOverlay && activeOverlay->type != EMPTY) {
        int cells[4][2];
        getPieceCells(activeOverlay->type, activeOverlay->rotation,
                      activeOverlay->x, activeOverlay->y, cells);
        for (int i = 0; i < 4; i++) {
            int r = cells[i][0];
            int c = cells[i][1];
            if (r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W) {
                drawCell(boardTop + 1 + r, boardLeft + 1 + c * CELL_W,
                         (uint8_t)activeOverlay->type);
            }
        }
    }

    /* 오른쪽 정보 영역 */
    int infoLeft = boardLeft + 1 + BOARD_W * CELL_W + 3;

    mvprintw(boardTop,     infoLeft, "SCORE");
    mvprintw(boardTop + 1, infoLeft, "%06u", score);

    mvprintw(boardTop + 3, infoLeft, "GARBAGE");
    if (gColors) attron(COLOR_PAIR(GARBAGE) | A_BOLD);
    mvprintw(boardTop + 4, infoLeft, "%2u", pendingGarbage);
    if (gColors) attroff(COLOR_PAIR(GARBAGE) | A_BOLD);

    /* NEXT 미리보기 5개 (bag 정보가 있을 때만) */
    if (bag) {
        mvprintw(boardTop + 6, infoLeft, "NEXT");
        for (int i = 0; i < 5; i++) {
            int idx = (int)bag->currentIndex + i;
            BlockType t;
            if (idx < 7) {
                t = bag->bag[idx];
            } else if (idx - 7 < 7) {
                t = bag->nextBag[idx - 7];
            } else {
                t = EMPTY;
            }
            if (t == EMPTY) continue;
            int cells[4][2];
            getPieceCells(t, 0, 0, 0, cells);
            int rowBase = boardTop + 7 + i * 3;
            /* 작은 공간(3행)에 그리기 위해 4x2 영역으로 압축해 그림.
             * 단순히 0~3 행/열 매핑을 그대로 사용해도 시각적으로 OK. */
            for (int j = 0; j < 4; j++) {
                int r = cells[j][0];
                int c = cells[j][1];
                if (r >= 0 && r < 3 && c >= 0 && c < 4) {
                    drawCell(rowBase + r, infoLeft + c * CELL_W, (uint8_t)t);
                }
            }
        }
    }
}

void uiRender(const GameState* me, const NetContext* netCtx)
{
    erase();

    /* 헤더 */
    const char* title = netCtx ? "TETRIS MULTIPLAYER (1v1)" : "TETRIS";
    mvprintw(0, 2, "%s", title);

    /* 본인 패널 */
    drawPlayerPanel(2, 2, "[ YOU ]",
                    me->board, &me->activeBlock, me->holdBlock, &me->bagState,
                    me->score, me->pendingGarbage);

    /* 상대 패널 */
    if (netCtx) {
        int rightLeft = 2 + 1 + 4 * CELL_W + 4 + 1 + BOARD_W * CELL_W + 12;
        const CurrentBlock* oppActive =
            netCtx->opponentHasActive ? &netCtx->opponentActive : NULL;
        drawPlayerPanel(2, rightLeft, "[ OPPONENT ]",
                        netCtx->opponentBoard,
                        oppActive,
                        EMPTY,          /* 상대 hold도 미전송 */
                        NULL,           /* 상대 NEXT도 미전송 */
                        netCtx->opponentScore,
                        0);
        /* 연결/오버 상태 표시 */
        const char* statusMsg;
        if (!netCtx->connected) statusMsg = "Status: DISCONNECTED";
        else if (netCtx->opponentLost) statusMsg = "Status: opponent LOST";
        else statusMsg = "Status: Connected";
        mvprintw(1, rightLeft, "%s", statusMsg);
    }

    /* 컨트롤 안내 */
    int controlsRow = 2 + 2 + BOARD_H + 2;
    mvprintw(controlsRow, 2,
             "Controls: < > Move   v Soft Drop   ^ Rotate   Space Hard Drop   C Hold   Q Quit");

    refresh();
}

void uiShowMessage(const char* msg)
{
    int controlsRow = 2 + 2 + BOARD_H + 4;
    move(controlsRow, 2);
    clrtoeol();
    mvprintw(controlsRow, 2, "%s", msg);
    refresh();
}

void uiWaitKey(void)
{
    nodelay(stdscr, FALSE);
    (void)getch();
    nodelay(stdscr, TRUE);
}
