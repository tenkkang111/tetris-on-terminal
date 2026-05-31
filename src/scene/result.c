#include "scene.h"
#include "ui.h"
#include <ncurses.h>
#include <string.h>
#include <unistd.h>

static void drawBox(int y, int x, int w, int h, const char* title, int colorPair)
{
    if (has_colors() && colorPair > 0) attron(COLOR_PAIR(colorPair));

    mvaddch(y, x, ACS_ULCORNER);
    for (int i = 1; i < w - 1; i++) mvaddch(y, x + i, ACS_HLINE);
    mvaddch(y, x + w - 1, ACS_URCORNER);

    for (int j = 1; j < h - 1; j++) {
        mvaddch(y + j, x, ACS_VLINE);
        for (int i = 1; i < w - 1; i++) mvaddch(y + j, x + i, ' ');
        mvaddch(y + j, x + w - 1, ACS_VLINE);
    }

    mvaddch(y + h - 1, x, ACS_LLCORNER);
    for (int i = 1; i < w - 1; i++) mvaddch(y + h - 1, x + i, ACS_HLINE);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

    if (has_colors() && colorPair > 0) attroff(COLOR_PAIR(colorPair));

    if (title) {
        int titleLen = (int)strlen(title);
        int titleX = x + (w - titleLen - 4) / 2;
        if (has_colors() && colorPair > 0) attron(COLOR_PAIR(colorPair) | A_BOLD);
        mvprintw(y, titleX, "[ %s ]", title);
        if (has_colors() && colorPair > 0) attroff(COLOR_PAIR(colorPair) | A_BOLD);
    }
}

static void drawSingleResult(const GameResult* r)
{
    int centerX = COLS / 2;
    int boxW = 40;
    int boxH = 16;
    int boxX = centerX - boxW / 2;
    int boxY = LINES / 2 - boxH / 2;

    drawBox(boxY, boxX, boxW, boxH, "GAME OVER", 7);

    const char* title = "SOLO GAME COMPLETE";
    if (has_colors()) attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(boxY + 2, centerX - (int)strlen(title) / 2, "%s", title);
    if (has_colors()) attroff(COLOR_PAIR(6) | A_BOLD);

    mvaddch(boxY + 4, boxX, ACS_LTEE);
    for (int i = 1; i < boxW - 1; i++) mvaddch(boxY + 4, boxX + i, ACS_HLINE);
    mvaddch(boxY + 4, boxX + boxW - 1, ACS_RTEE);

    int row = boxY + 6;
    mvprintw(row, boxX + 6, "SCORE");
    if (has_colors()) attron(A_BOLD);
    mvprintw(row, boxX + boxW - 14, "%10u", r->myScore);
    if (has_colors()) attroff(A_BOLD);

    row += 2;
    mvprintw(row, boxX + 6, "LINES");
    mvprintw(row, boxX + boxW - 14, "%10u", r->myLines);

    row += 2;
    mvprintw(row, boxX + 6, "LEVEL");
    mvprintw(row, boxX + boxW - 14, "%10u", r->myLevel);
}

static void drawMultiResult(const GameResult* r)
{
    int centerX = COLS / 2;
    int boxW = 50;
    int boxH = 18;
    int boxX = centerX - boxW / 2;
    int boxY = LINES / 2 - boxH / 2;

    const char* resultText;
    int resultColor;

    if (r->isDisconnect) {
        resultText = "DISCONNECTED";
        resultColor = 4;
    } else if (r->isWin) {
        resultText = "VICTORY";
        resultColor = 5;
    } else {
        resultText = "DEFEAT";
        resultColor = 7;
    }

    drawBox(boxY, boxX, boxW, boxH, "MATCH RESULT", resultColor);

    if (has_colors()) attron(COLOR_PAIR(resultColor) | A_BOLD);
    mvprintw(boxY + 2, centerX - (int)strlen(resultText) / 2, "%s", resultText);
    if (has_colors()) attroff(COLOR_PAIR(resultColor) | A_BOLD);

    mvaddch(boxY + 4, boxX, ACS_LTEE);
    for (int i = 1; i < boxW - 1; i++) mvaddch(boxY + 4, boxX + i, ACS_HLINE);
    mvaddch(boxY + 4, boxX + boxW - 1, ACS_RTEE);

    int row = boxY + 6;
    mvprintw(row, boxX + 14, "YOU");
    mvprintw(row, boxX + boxW - 14, "OPP");

    row += 1;
    mvaddch(row, boxX, ACS_LTEE);
    for (int i = 1; i < boxW - 1; i++) mvaddch(row, boxX + i, ACS_HLINE);
    mvaddch(row, boxX + boxW - 1, ACS_RTEE);

    row += 2;
    mvprintw(row, boxX + 4, "SCORE");
    bool myScoreWin = (r->myScore >= r->oppScore);
    if (myScoreWin && has_colors()) attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row, boxX + 12, "%7u", r->myScore);
    if (myScoreWin && has_colors()) attroff(COLOR_PAIR(5) | A_BOLD);

    if (!myScoreWin && has_colors()) attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row, boxX + boxW - 12, "%7u", r->oppScore);
    if (!myScoreWin && has_colors()) attroff(COLOR_PAIR(5) | A_BOLD);

    row += 2;
    mvprintw(row, boxX + 4, "LINES");
    bool myLinesWin = (r->myLines >= r->oppLines);
    if (myLinesWin && has_colors()) attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row, boxX + 12, "%7u", r->myLines);
    if (myLinesWin && has_colors()) attroff(COLOR_PAIR(5) | A_BOLD);

    if (!myLinesWin && has_colors()) attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row, boxX + boxW - 12, "%7u", r->oppLines);
    if (!myLinesWin && has_colors()) attroff(COLOR_PAIR(5) | A_BOLD);

    row += 2;
    mvprintw(row, boxX + 4, "LEVEL");
    mvprintw(row, boxX + 12, "%7u", r->myLevel);
    mvprintw(row, boxX + boxW - 12, "   -");
}

SceneType sceneResult(SceneContext* ctx)
{
    nodelay(stdscr, TRUE);
    curs_set(0);

    const GameResult* r = &ctx->result;

    while (1) {
        erase();

        if (r->isSinglePlayer) {
            drawSingleResult(r);
        } else {
            drawMultiResult(r);
        }

        int bottomY = LINES - 3;
        const char* hint1 = "Press ENTER to return to lobby";
        const char* hint2 = "Press Q to quit";

        if (has_colors()) attron(A_DIM);
        mvprintw(bottomY, COLS / 2 - (int)strlen(hint1) / 2, "%s", hint1);
        mvprintw(bottomY + 1, COLS / 2 - (int)strlen(hint2) / 2, "%s", hint2);
        if (has_colors()) attroff(A_DIM);

        refresh();

        int ch = getch();
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            return SCENE_LOBBY;
        }
        if (ch == 'q' || ch == 'Q') {
            return SCENE_QUIT;
        }

        usleep(16000);
    }
}
