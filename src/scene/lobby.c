#include "scene.h"
#include "ui.h"
#include <ncurses.h>
#include <string.h>
#include <unistd.h>

static const char* LOGO[] = {
    " __               __                            ",
    "/\\ \\__           /\\ \\__           __            ",
    "\\ \\ ,_\\     __   \\ \\ ,_\\   _ __  /\\_\\     ____  ",
    " \\ \\ \\/   /'__`\\  \\ \\ \\/  /\\`'__\\\\/\\ \\   /',__\\ ",
    "  \\ \\ \\_ /\\  __/   \\ \\ \\_ \\ \\ \\/  \\ \\ \\ /\\__, `\\",
    "   \\ \\__\\\\ \\____\\   \\ \\__\\ \\ \\_\\   \\ \\_\\\\/\\____/",
    "    \\/__/ \\/____/    \\/__/  \\/_/    \\/_/ \\/___/ ",
    "",
    NULL
};

typedef enum {
    MENU_SINGLE = 0,
    MENU_HOST,
    MENU_FIND,
    MENU_COUNT
} MenuItem;

static const char* MENU_LABELS[] = {
    "SOLO",
    "HOST GAME",
    "FIND GAME"
};

static const char* MENU_DESC[] = {
    "Play alone with augment system",
    "Host a room for LAN players",
    "Search for games on local network"
};

static void drawMenuBox(int y, int x, int w, int h, bool selected)
{
    chtype ul = selected ? ACS_ULCORNER : ACS_ULCORNER;
    chtype ur = selected ? ACS_URCORNER : ACS_URCORNER;
    chtype ll = selected ? ACS_LLCORNER : ACS_LLCORNER;
    chtype lr = selected ? ACS_LRCORNER : ACS_LRCORNER;
    chtype hl = selected ? ACS_HLINE : ACS_HLINE;
    chtype vl = selected ? ACS_VLINE : ACS_VLINE;

    if (selected) attron(A_BOLD);

    mvaddch(y, x, ul);
    for (int i = 1; i < w - 1; i++) mvaddch(y, x + i, hl);
    mvaddch(y, x + w - 1, ur);

    for (int j = 1; j < h - 1; j++) {
        mvaddch(y + j, x, vl);
        for (int i = 1; i < w - 1; i++) mvaddch(y + j, x + i, ' ');
        mvaddch(y + j, x + w - 1, vl);
    }

    mvaddch(y + h - 1, x, ll);
    for (int i = 1; i < w - 1; i++) mvaddch(y + h - 1, x + i, hl);
    mvaddch(y + h - 1, x + w - 1, lr);

    if (selected) attroff(A_BOLD);
}

SceneType sceneLobby(SceneContext* ctx)
{
    int selected = MENU_SINGLE;

    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    while (1) {
        erase();

        int centerX = COLS / 2;
        int startY = 2;

        if (has_colors()) attron(COLOR_PAIR(6) | A_BOLD);
        for (int i = 0; LOGO[i] != NULL; i++) {
            int logoLen = (int)strlen(LOGO[i]);
            mvprintw(startY + i, centerX - logoLen / 2, "%s", LOGO[i]);
        }
        if (has_colors()) attroff(COLOR_PAIR(6) | A_BOLD);

        mvprintw(startY + 6, centerX - 5, "v1.0.0");

        int menuY = startY + 9;
        int menuW = 30;
        int menuH = 5;
        int menuGap = 1;
        int menuStartX = centerX - menuW / 2;

        for (int i = 0; i < MENU_COUNT; i++) {
            int itemY = menuY + i * (menuH + menuGap);
            bool isSel = (i == selected);

            if (isSel && has_colors()) attron(COLOR_PAIR(1));
            drawMenuBox(itemY, menuStartX, menuW, menuH, isSel);

            int labelLen = (int)strlen(MENU_LABELS[i]);
            if (isSel) attron(A_BOLD);
            mvprintw(itemY + 1, centerX - labelLen / 2, "%s", MENU_LABELS[i]);
            if (isSel) attroff(A_BOLD);

            int descLen = (int)strlen(MENU_DESC[i]);
            if (has_colors()) attron(A_DIM);
            mvprintw(itemY + 2, centerX - descLen / 2, "%s", MENU_DESC[i]);
            if (has_colors()) attroff(A_DIM);

            if (isSel && has_colors()) attroff(COLOR_PAIR(1));
        }

        int selY = menuY + selected * (menuH + menuGap) + 1;
        if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(selY, menuStartX + 2, ">");
        mvprintw(selY, menuStartX + menuW - 3, "<");
        if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);

        int bottomY = LINES - 2;
        const char* hint = "UP/DOWN: Navigate   ENTER: Select   Q: Quit";
        mvprintw(bottomY, centerX - (int)strlen(hint) / 2, "%s", hint);

        refresh();

        int ch = getch();
        switch (ch) {
            case KEY_UP:
            case 'w':
            case 'W':
                selected = (selected - 1 + MENU_COUNT) % MENU_COUNT;
                break;
            case KEY_DOWN:
            case 's':
            case 'S':
                selected = (selected + 1) % MENU_COUNT;
                break;
            case '\n':
            case '\r':
            case KEY_ENTER:
                switch (selected) {
                    case MENU_SINGLE:
                        ctx->netMode = NET_MODE_NONE;
                        return SCENE_GAME;
                    case MENU_HOST:
                        ctx->netMode = NET_MODE_HOST;
                        return SCENE_MATCHING;
                    case MENU_FIND:
                        ctx->netMode = NET_MODE_CLIENT;
                        ctx->hostIp[0] = '\0';
                        return SCENE_MATCHING;
                }
                break;
            case 'q':
            case 'Q':
                return SCENE_QUIT;
            case '1':
                ctx->netMode = NET_MODE_NONE;
                return SCENE_GAME;
            case '2':
                ctx->netMode = NET_MODE_HOST;
                return SCENE_MATCHING;
            case '3':
                ctx->netMode = NET_MODE_CLIENT;
                ctx->hostIp[0] = '\0';
                return SCENE_MATCHING;
        }

        usleep(16000);
    }
}
