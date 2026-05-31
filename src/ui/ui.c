#include "ui.h"
#include "core.h"
#include "augment.h"

#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BOARD_W 10
#define BOARD_H 20
#define CELL_W  2
#define NEXT_DISPLAY_CAP 7

#define MIN_COLS_SINGLE 55
#define MIN_ROWS_SINGLE 28
#define MIN_COLS_MULTI  115
#define MIN_ROWS_MULTI  28

#define TIER_PAIR_BRONZE 20
#define TIER_PAIR_SILVER 21
#define TIER_PAIR_GOLD   22
#define TIER_PAIR_PRISM  23

static bool gColors = false;

static int colorForType(BlockType t) { return (int)t; }

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

        init_pair(TIER_PAIR_BRONZE, COLOR_YELLOW,  -1);
        init_pair(TIER_PAIR_SILVER, COLOR_WHITE,   -1);
        init_pair(TIER_PAIR_GOLD,   COLOR_YELLOW,  -1);
        init_pair(TIER_PAIR_PRISM,  COLOR_MAGENTA, -1);
        gColors = true;
    }

    mousemask(BUTTON1_PRESSED | BUTTON1_RELEASED | BUTTON1_CLICKED, NULL);
    mouseinterval(0);
}

bool uiCheckScreenSize(bool isMultiplayer)
{
    int minCols = isMultiplayer ? MIN_COLS_MULTI : MIN_COLS_SINGLE;
    int minRows = isMultiplayer ? MIN_ROWS_MULTI : MIN_ROWS_SINGLE;

    if (COLS < minCols || LINES < minRows) {
        clear();
        mvprintw(0, 0, "WARNING: Terminal too small!");
        mvprintw(1, 0, "Current: %dx%d, Required: %dx%d", COLS, LINES, minCols, minRows);
        mvprintw(3, 0, "Please resize your terminal window.");
        mvprintw(4, 0, "Press any key to continue anyway, or Q to quit.");
        refresh();
        nodelay(stdscr, FALSE);
        int ch = getch();
        nodelay(stdscr, TRUE);
        if (ch == 'q' || ch == 'Q') {
            return false;
        }
    }
    return true;
}

bool uiIsScreenSizeOk(bool isMultiplayer)
{
    int minCols = isMultiplayer ? MIN_COLS_MULTI : MIN_COLS_SINGLE;
    int minRows = isMultiplayer ? MIN_ROWS_MULTI : MIN_ROWS_SINGLE;
    return (COLS >= minCols && LINES >= minRows);
}

void uiDrawPauseOverlay(bool isMultiplayer)
{
    int minCols = isMultiplayer ? MIN_COLS_MULTI : MIN_COLS_SINGLE;
    int minRows = isMultiplayer ? MIN_ROWS_MULTI : MIN_ROWS_SINGLE;

    erase();

    bool needWidth = (COLS < minCols);
    bool needHeight = (LINES < minRows);

    /* Help Grid */
    if (gColors) attron(COLOR_PAIR(5) | A_DIM);

    for (int x = 0; x < COLS && x < minCols; x++) {
        mvaddch(0, x, '-');
    }

    if (minRows - 1 < LINES) {
        for (int x = 0; x < COLS && x < minCols; x++) {
            mvaddch(minRows - 1, x, '-');
        }
    }

    for (int y = 0; y < LINES && y < minRows; y++) {
        mvaddch(y, 0, '|');
    }

    if (minCols - 1 < COLS) {
        for (int y = 0; y < LINES && y < minRows; y++) {
            mvaddch(y, minCols - 1, '|');
        }
    }

    mvaddch(0, 0, '+');
    if (minCols - 1 < COLS) mvaddch(0, minCols - 1, '+');
    if (minRows - 1 < LINES) mvaddch(minRows - 1, 0, '+');
    if (minCols - 1 < COLS && minRows - 1 < LINES) mvaddch(minRows - 1, minCols - 1, '+');

    if (gColors) attroff(COLOR_PAIR(5) | A_DIM);

    /* Direction Arrows */
    if (gColors) attron(COLOR_PAIR(7) | A_BOLD);

    if (needWidth) {
        int arrowX = COLS - 1;
        for (int y = 1; y < LINES - 1; y += 2) {
            mvaddch(y, arrowX, '>');
        }
        if (COLS > 12) {
            char widthHint[24];
            snprintf(widthHint, sizeof(widthHint), "+%d cols", minCols - COLS);
            int hintLen = (int)strlen(widthHint);
            mvprintw(1, COLS - hintLen - 1, "%s", widthHint);
        }
    }

    if (needHeight) {
        int arrowY = LINES - 1;
        for (int x = 2; x < COLS - 1; x += 3) {
            mvaddch(arrowY, x, 'v');
        }
        if (COLS > 12) {
            char heightHint[24];
            snprintf(heightHint, sizeof(heightHint), "+%d rows", minRows - LINES);
            mvprintw(LINES - 1, 1, "%s", heightHint);
        }
    }

    if (gColors) attroff(COLOR_PAIR(7) | A_BOLD);

    /* Center Box */
    int boxW = 34;
    int boxH = 7;
    int boxX = (COLS - boxW) / 2;
    int boxY = (LINES - boxH) / 2;
    if (boxX < 1) boxX = 1;
    if (boxY < 1) boxY = 1;
    if (boxX + boxW > COLS - 1) boxW = COLS - boxX - 1;
    if (boxY + boxH > LINES - 1) boxH = LINES - boxY - 1;

    if (boxW < 18 || boxH < 5) {
        if (gColors) attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(LINES / 2, 1, "RESIZE!");
        if (gColors) attroff(COLOR_PAIR(4) | A_BOLD);
        refresh();
        return;
    }

    for (int y = boxY; y < boxY + boxH; y++) {
        for (int x = boxX; x < boxX + boxW; x++) {
            mvaddch(y, x, ' ');
        }
    }

    if (gColors) attron(COLOR_PAIR(4) | A_BOLD);
    for (int x = boxX; x < boxX + boxW; x++) {
        mvaddch(boxY, x, ACS_HLINE);
        mvaddch(boxY + boxH - 1, x, ACS_HLINE);
    }
    for (int y = boxY; y < boxY + boxH; y++) {
        mvaddch(y, boxX, ACS_VLINE);
        mvaddch(y, boxX + boxW - 1, ACS_VLINE);
    }
    mvaddch(boxY, boxX, ACS_ULCORNER);
    mvaddch(boxY, boxX + boxW - 1, ACS_URCORNER);
    mvaddch(boxY + boxH - 1, boxX, ACS_LLCORNER);
    mvaddch(boxY + boxH - 1, boxX + boxW - 1, ACS_LRCORNER);
    if (gColors) attroff(COLOR_PAIR(4) | A_BOLD);

    int row = boxY + 1;
    const char* title = "GAME PAUSED";
    int titleX = boxX + (boxW - (int)strlen(title)) / 2;
    if (gColors) attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(row, titleX, "%s", title);
    if (gColors) attroff(COLOR_PAIR(4) | A_BOLD);

    row += 2;
    char sizeBuf[36];
    snprintf(sizeBuf, sizeof(sizeBuf), "Need %dx%d  Now %dx%d", minCols, minRows, COLS, LINES);
    int sizeX = boxX + (boxW - (int)strlen(sizeBuf)) / 2;
    mvprintw(row, sizeX, "%s", sizeBuf);

    row += 2;
    const char* hint = "Resize to continue";
    int hintX = boxX + (boxW - (int)strlen(hint)) / 2;
    if (gColors) attron(A_DIM);
    mvprintw(row, hintX, "%s", hint);
    if (gColors) attroff(A_DIM);

    refresh();
}

void uiShutdown(void) { endwin(); }

UiKey uiPollKey(void)
{
    int ch = getch();
    switch (ch) {
        case ERR:           return UI_KEY_NONE;
        case KEY_LEFT:      return UI_KEY_LEFT;
        case KEY_RIGHT:     return UI_KEY_RIGHT;
        case KEY_DOWN:      return UI_KEY_SOFT_DROP;
        case KEY_UP:
        case 'x': case 'X': return UI_KEY_ROTATE_CW;
        case 'z': case 'Z': return UI_KEY_ROTATE_CCW;
        case 'a': case 'A': return UI_KEY_ROTATE_180;
        case ' ':           return UI_KEY_HARD_DROP;
        case 'c': case 'C': return UI_KEY_HOLD;
        case 'q': case 'Q': return UI_KEY_QUIT;
        case KEY_MOUSE: {
            MEVENT ev;
            (void)getmouse(&ev);
            return UI_KEY_NONE;
        }
        default:            return UI_KEY_NONE;
    }
}

static int tierColorPair(Tier t)
{
    switch (t) {
        case TIER_BRONZE: return TIER_PAIR_BRONZE;
        case TIER_SILVER: return TIER_PAIR_SILVER;
        case TIER_GOLD:   return TIER_PAIR_GOLD;
        case TIER_PRISM:  return TIER_PAIR_PRISM;
        default: return 0;
    }
}

static int tierAttrs(Tier t)
{
    switch (t) {
        case TIER_BRONZE: return A_DIM;
        case TIER_SILVER: return A_BOLD;
        case TIER_GOLD:   return A_BOLD;
        case TIER_PRISM:  return A_BOLD | A_BLINK;
        default: return 0;
    }
}

static void drawCell(int row, int col, uint8_t type)
{
    if (type == EMPTY) { mvaddstr(row, col, " ."); return; }
    int cp = colorForType((BlockType)type);
    if (gColors) attron(COLOR_PAIR(cp) | A_BOLD);
    if (type == GARBAGE) mvaddstr(row, col, "##");
    else                  mvaddstr(row, col, "[]");
    if (gColors) attroff(COLOR_PAIR(cp) | A_BOLD);
}

static void drawGhostCell(int row, int col, BlockType type)
{
    int cp = colorForType(type);
    if (gColors) attron(COLOR_PAIR(cp) | A_DIM);
    mvaddstr(row, col, "::");
    if (gColors) attroff(COLOR_PAIR(cp) | A_DIM);
}

static void drawMiniPiece(int top, int left, BlockType type)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            mvaddstr(top + r, left + c * CELL_W, "  ");
    if (type == EMPTY) return;

    int cells[4][2];
    getPieceCells(type, 0, 0, 0, cells);
    for (int i = 0; i < 4; i++) {
        int r = cells[i][0], c = cells[i][1];
        if (r >= 0 && r < 4 && c >= 0 && c < 4)
            drawCell(top + r, left + c * CELL_W, (uint8_t)type);
    }
}

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

static void drawPlayerPanel(int topRow, int leftCol, const char* title,
                            const uint8_t board[20][10],
                            const CurrentBlock* activeOverlay,
                            int ghostY,
                            BlockType holdBlock,
                            const BagSystem* bag,
                            int nextCount,
                            uint32_t score,
                            uint8_t pendingGarbage,
                            int b2b, int combo, long totalLines,
                            int level, int xp, int xpToNext,
                            float lockProgress)
{
    mvprintw(topRow, leftCol, "%s", title);

    /* Level/XP Bar */
    if (level >= 0) {
        mvprintw(topRow + 1, leftCol, "LV %d", level);
        if (xpToNext > 0) {
            int barW = 10;
            int filled = (xp * barW) / xpToNext;
            if (filled > barW) filled = barW;
            mvprintw(topRow + 1, leftCol + 6, "[");
            for (int i = 0; i < barW; i++) {
                addch(i < filled ? '#' : '.');
            }
            mvprintw(topRow + 1, leftCol + 6 + barW + 1, "] %d/%d", xp, xpToNext);
        } else {
            mvprintw(topRow + 1, leftCol + 6, "[MAX]");
        }
    }

    /* HOLD */
    int holdTop  = topRow + 3;
    int holdLeft = leftCol + 1;
    drawMiniFrame(holdTop, holdLeft, "HOLD");
    drawMiniPiece(holdTop + 1, holdLeft, holdBlock);

    /* HUD */
    int hudTop = holdTop + 7;
    int hudLeft = leftCol;

    mvprintw(hudTop, hudLeft, "SCORE");
    if (gColors) attron(A_BOLD);
    mvprintw(hudTop + 1, hudLeft, "%06u", score);
    if (gColors) attroff(A_BOLD);

    if (totalLines >= 0) {
        mvprintw(hudTop + 3, hudLeft, "LINES");
        mvprintw(hudTop + 4, hudLeft, "%05ld", totalLines);
    }

    mvprintw(hudTop + 6, hudLeft, "GARBAGE");
    if (gColors) attron(COLOR_PAIR(GARBAGE) | A_BOLD);
    mvprintw(hudTop + 7, hudLeft, "%2u", pendingGarbage);
    if (gColors) attroff(COLOR_PAIR(GARBAGE) | A_BOLD);

    if (b2b >= 0 && b2b > 0)
        mvprintw(hudTop + 9, hudLeft, "B2B x%d", b2b);
    else
        mvprintw(hudTop + 9, hudLeft, "       ");

    if (combo >= 0 && combo > 1)
        mvprintw(hudTop + 10, hudLeft, "CMB x%d", combo);
    else
        mvprintw(hudTop + 10, hudLeft, "       ");

    /* Board */
    int boardTop  = topRow + 2;
    int boardLeft = leftCol + 12;
    drawBoardFrame(boardTop, boardLeft);
    for (int r = 0; r < BOARD_H; r++)
        for (int c = 0; c < BOARD_W; c++)
            drawCell(boardTop + 1 + r, boardLeft + 1 + c * CELL_W, board[r][c]);

    /* Ghost Piece */
    if (activeOverlay && activeOverlay->type != EMPTY && ghostY >= 0
        && ghostY != activeOverlay->y) {
        int cells[4][2];
        getPieceCells(activeOverlay->type, activeOverlay->rotation,
                      activeOverlay->x, ghostY, cells);
        for (int i = 0; i < 4; i++) {
            int r = cells[i][0], c = cells[i][1];
            if (r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W)
                drawGhostCell(boardTop + 1 + r, boardLeft + 1 + c * CELL_W,
                              activeOverlay->type);
        }
    }

    /* Active Piece */
    if (activeOverlay && activeOverlay->type != EMPTY) {
        int cells[4][2];
        getPieceCells(activeOverlay->type, activeOverlay->rotation,
                      activeOverlay->x, activeOverlay->y, cells);
        for (int i = 0; i < 4; i++) {
            int r = cells[i][0], c = cells[i][1];
            if (r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W)
                drawCell(boardTop + 1 + r, boardLeft + 1 + c * CELL_W,
                         (uint8_t)activeOverlay->type);
        }
    }

    /* Lock Delay Bar */
    int lockRow = boardTop + BOARD_H + 2;
    if (lockProgress >= 0.0f) {
        int barW = 10;
        int filled = (int)(lockProgress * barW);
        if (filled > barW) filled = barW;
        mvprintw(lockRow, boardLeft + 1, "LOCK [");
        for (int i = 0; i < barW; i++) {
            if (i < filled) {
                if (gColors) {
                    if (lockProgress < 0.5f) attron(COLOR_PAIR(S) | A_BOLD);
                    else if (lockProgress < 0.8f) attron(COLOR_PAIR(L) | A_BOLD);
                    else attron(COLOR_PAIR(Z) | A_BOLD);
                }
                addch('#');
                if (gColors) attroff(COLOR_PAIR(S) | COLOR_PAIR(L) | COLOR_PAIR(Z) | A_BOLD);
            } else {
                addch('.');
            }
        }
        addch(']');
    } else {
        mvprintw(lockRow, boardLeft + 1, "                  ");
    }

    /* NEXT */
    int nextLeft = boardLeft + 1 + BOARD_W * CELL_W + 3;
    int nextTop = boardTop;

    if (bag && nextCount > 0) {
        if (nextCount > NEXT_DISPLAY_CAP) nextCount = NEXT_DISPLAY_CAP;

        mvprintw(nextTop, nextLeft, "NEXT");

        int maxVisible = (BOARD_H - 1) / 3;
        if (nextCount > maxVisible) nextCount = maxVisible;

        for (int i = 0; i < nextCount; i++) {
            int idx = (int)bag->currentIndex + i;
            BlockType t;
            if (idx < 7)              t = bag->bag[idx];
            else if (idx - 7 < 7)     t = bag->nextBag[idx - 7];
            else                       t = EMPTY;
            if (t == EMPTY) continue;

            int cells[4][2];
            getPieceCells(t, 0, 0, 0, cells);
            int rowBase = nextTop + 2 + i * 3;

            for (int j = 0; j < 4; j++) {
                int r = cells[j][0], c = cells[j][1];
                if (r >= 0 && r < 2 && c >= 0 && c < 4)
                    drawCell(rowBase + r, nextLeft + c * CELL_W, (uint8_t)t);
            }
        }
    }
}

static void drawAugmentList(int row, int leftCol, const AugInventory* inv)
{
    if (!inv || inv->count <= 0) return;
    mvprintw(row, leftCol, "EQUIPPED:");
    int col = leftCol + 11;
    int curRow = row;
    for (int i = 0; i < inv->count; i++) {
        OwnedAugment a = inv->list[i];
        const AugmentDef* d = augmentDef(a.id);
        char desc[40];
        augDescribe(a, desc, sizeof(desc));
        char buf[80];
        snprintf(buf, sizeof(buf), "[%c] %s %s",
                 tierShortChar(a.tier), d ? d->name : "?", desc);

        int needed = (int)strlen(buf);
        if (col + needed > COLS - 2) {
            curRow++;
            col = leftCol + 11;
        }
        int cp = tierColorPair(a.tier);
        int attrs = tierAttrs(a.tier);
        if (cp > 0) attron(COLOR_PAIR(cp) | attrs);
        mvprintw(curRow, col, "%s", buf);
        if (cp > 0) attroff(COLOR_PAIR(cp) | attrs);
        col += needed + 3;
    }
}

void uiRender(const GameState* me, const NetContext* netCtx,
              const AugInventory* augInv, const LevelState* lvl,
              float lockProgress)
{
    erase();

    const char* title = netCtx ? "TETRIS MULTIPLAYER (1v1)" : "TETRIS";
    mvprintw(0, 2, "%s", title);

    int myLvl = -1, myXp = 0, myXpNext = 0;
    if (lvl) { myLvl = lvl->level; myXp = lvl->xp; myXpNext = lvl->xpToNext; }

    int myNextCount = 5 + (augInv ? augInv->nextVisionBonus : 0);

    int ghostY = ghostDropY(me);
    drawPlayerPanel(2, 2, "[ YOU ]",
                    me->board, &me->activeBlock, ghostY,
                    me->holdBlock, &me->bagState, myNextCount,
                    me->score, me->pendingGarbage,
                    (int)me->b2b, (int)me->combo, (long)me->totalLines,
                    myLvl, myXp, myXpNext, lockProgress);

    if (netCtx) {
        int rightLeft = 2 + 12 + (BOARD_W * CELL_W + 2) + 10 + 4;
        const CurrentBlock* oppActive =
            netCtx->opponentHasActive ? &netCtx->opponentActive : NULL;
        int oppGhost = -1;
        if (oppActive) {
            int gy = oppActive->y;
            int cells[4][2];
            while (1) {
                bool collide = false;
                getPieceCells(oppActive->type, oppActive->rotation,
                              oppActive->x, gy + 1, cells);
                for (int i = 0; i < 4; i++) {
                    int r = cells[i][0], c = cells[i][1];
                    if (c < 0 || c >= BOARD_W || r >= BOARD_H) { collide = true; break; }
                    if (r >= 0 && netCtx->opponentBoard[r][c] != EMPTY) { collide = true; break; }
                }
                if (collide) break;
                gy++;
            }
            oppGhost = gy;
        }
        drawPlayerPanel(2, rightLeft, "[ OPPONENT ]",
                        netCtx->opponentBoard, oppActive, oppGhost,
                        netCtx->opponentHold, &netCtx->opponentBag, 5,
                        netCtx->opponentScore, netCtx->opponentPendingGarbage,
                        (int)netCtx->opponentB2b, (int)netCtx->opponentCombo,
                        (long)netCtx->opponentTotalLines,
                        -1, 0, 0, -1.0f);
    }

    int controlsRow = 2 + 2 + BOARD_H + 2;
    mvprintw(controlsRow, 2,
             "Controls: < > Move   v Soft   X/^ CW   Z CCW   A 180   Space Hard   C Hold   Q Quit");

    drawAugmentList(controlsRow + 2, 2, augInv);

    refresh();
}

/* ---- Card Modal ---- */

#define CARD_W 22
#define CARD_H 11
#define CARD_GAP 3

typedef struct { int x, y, w, h; } Rect;

static void drawCard(Rect r, OwnedAugment a, int hotkey, bool hover)
{
    int cp = tierColorPair(a.tier);
    int attrs = tierAttrs(a.tier);

    chtype hl = ACS_HLINE, vl = ACS_VLINE;
    if (hover) { hl = '='; vl = '|'; }

    mvaddch(r.y, r.x, ACS_ULCORNER);
    for (int i = 0; i < r.w - 2; i++) mvaddch(r.y, r.x + 1 + i, hl);
    mvaddch(r.y, r.x + r.w - 1, ACS_URCORNER);
    for (int j = 0; j < r.h - 2; j++) {
        mvaddch(r.y + 1 + j, r.x, vl);
        mvaddch(r.y + 1 + j, r.x + r.w - 1, vl);
        for (int i = 0; i < r.w - 2; i++) mvaddch(r.y + 1 + j, r.x + 1 + i, ' ');
    }
    mvaddch(r.y + r.h - 1, r.x, ACS_LLCORNER);
    for (int i = 0; i < r.w - 2; i++) mvaddch(r.y + r.h - 1, r.x + 1 + i, hl);
    mvaddch(r.y + r.h - 1, r.x + r.w - 1, ACS_LRCORNER);

    if (cp > 0) attron(COLOR_PAIR(cp) | attrs);
    const char* tname = tierName(a.tier);
    int tw = (int)strlen(tname);
    mvprintw(r.y + 2, r.x + (r.w - tw) / 2, "%s", tname);
    if (cp > 0) attroff(COLOR_PAIR(cp) | attrs);

    for (int i = 0; i < r.w - 2; i++) mvaddch(r.y + 3, r.x + 1 + i, ACS_HLINE);

    const AugmentDef* d = augmentDef(a.id);
    const char* nm = d ? d->name : "?";
    int nw = (int)strlen(nm);
    if (cp > 0) attron(COLOR_PAIR(cp));
    mvprintw(r.y + 5, r.x + (r.w - nw) / 2, "%s", nm);
    if (cp > 0) attroff(COLOR_PAIR(cp));

    char desc[64];
    augDescribe(a, desc, sizeof(desc));
    int dw = (int)strlen(desc);
    mvprintw(r.y + 7, r.x + (r.w - dw) / 2, "%s", desc);

    char k[6];
    snprintf(k, sizeof(k), "[ %d ]", hotkey);
    int kw = (int)strlen(k);
    mvprintw(r.y + r.h - 2, r.x + (r.w - kw) / 2, "%s", k);
}

int uiCardSelectModal(const GameState* me, NetContext* netCtx,
                      const AugInventory* augInv, const LevelState* lvl,
                      const CardOffer* offer)
{
    (void)augInv;
    int totalW = 3 * CARD_W + 2 * CARD_GAP;
    int startX = (COLS - totalW) / 2;
    if (startX < 1) startX = 1;
    int startY = (LINES - CARD_H) / 2;
    if (startY < 4) startY = 4;

    Rect cardR[3];
    for (int i = 0; i < 3; i++) {
        cardR[i].x = startX + i * (CARD_W + CARD_GAP);
        cardR[i].y = startY;
        cardR[i].w = CARD_W;
        cardR[i].h = CARD_H;
    }

    int selected = -1;
    int hoverIdx = -1;

    while (selected < 0) {
        erase();

        if (lvl) {
            char lbuf[32];
            snprintf(lbuf, sizeof(lbuf), "LEVEL %d", lvl->level);
            int lw = (int)strlen(lbuf);
            mvprintw(startY - 4, (COLS - lw) / 2, "%s", lbuf);
        }

        const char* banner = "*** LEVEL UP! Pick a card ***";
        int bw = (int)strlen(banner);
        if (gColors) attron(COLOR_PAIR(TIER_PAIR_PRISM) | A_BOLD);
        mvprintw(startY - 2, (COLS - bw) / 2, "%s", banner);
        if (gColors) attroff(COLOR_PAIR(TIER_PAIR_PRISM) | A_BOLD);

        for (int i = 0; i < 3; i++)
            drawCard(cardR[i], offer->offers[i], i + 1, hoverIdx == i);

        const char* hint = "Click a card or press 1 / 2 / 3      ( Q to quit )";
        int hw = (int)strlen(hint);
        mvprintw(startY + CARD_H + 1, (COLS - hw) / 2, "%s", hint);

        refresh();

        int ch;
        while ((ch = getch()) != ERR) {
            if (ch == '1') { selected = 0; break; }
            if (ch == '2') { selected = 1; break; }
            if (ch == '3') { selected = 2; break; }
            if (ch == 'q' || ch == 'Q') { selected = -1; goto done; }
            if (ch == KEY_MOUSE) {
                MEVENT ev;
                if (getmouse(&ev) == OK) {
                    int hit = -1;
                    for (int i = 0; i < 3; i++) {
                        if (ev.x >= cardR[i].x && ev.x < cardR[i].x + cardR[i].w &&
                            ev.y >= cardR[i].y && ev.y < cardR[i].y + cardR[i].h) {
                            hit = i; break;
                        }
                    }
                    if (hit >= 0 && (ev.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON1_PRESSED))) {
                        selected = hit;
                        break;
                    }
                    hoverIdx = hit;
                }
            }
        }

        if (netCtx && netCtx->mode != NET_MODE_NONE) {
            netPoll(netCtx, (GameState*)me);
        }

        napms(40);
    }

done:
    return selected;
}

void uiShowMessage(const char* msg)
{
    int controlsRow = 2 + 2 + BOARD_H + 5;
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
