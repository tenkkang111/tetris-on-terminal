#include "scene.h"
#include "ui.h"
#include <ncurses.h>
#include <string.h>
#include <unistd.h>

/* 기본 중계 서버 — ONLINE GAME 선택 시 자동 접속 */
#define DEFAULT_RELAY_IP   "3.26.115.31"
#define DEFAULT_RELAY_PORT 443

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
    MENU_ONLINE,
    MENU_COUNT
} MenuItem;

static const char* MENU_LABELS[] = {
    "SOLO",
    "HOST GAME",
    "FIND GAME",
    "ONLINE GAME"
};

static const char* MENU_DESC[] = {
    "Play alone with augment system",
    "Host a room for LAN players",
    "Search for games on local network",
    "Connect via relay server (cross-NAT)"
};

/* 선택된 항목은 굵게(A_BOLD) 그린다. 그 외에는 테두리 모양이 동일하다. */
static void drawMenuBox(int y, int x, int w, int h, bool selected)
{
    if (selected) attron(A_BOLD);

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

    if (selected) attroff(A_BOLD);
}

/* 선택한 메뉴 항목에 맞춰 컨텍스트를 설정하고 다음 씬을 반환한다.
 * ENTER와 숫자 단축키(1~4)가 동일한 경로를 타도록 공용화. */
static SceneType lobbySelect(SceneContext* ctx, int item)
{
    switch (item) {
        case MENU_SINGLE:
            ctx->netMode = NET_MODE_NONE;
            ctx->useServer = false;
            return SCENE_GAME;
        case MENU_HOST:
            ctx->netMode = NET_MODE_HOST;
            ctx->useServer = false;
            return SCENE_MATCHING;
        case MENU_FIND:
            ctx->netMode = NET_MODE_CLIENT;
            ctx->useServer = false;
            ctx->hostIp[0] = '\0';
            return SCENE_MATCHING;
        case MENU_ONLINE:
            ctx->netMode = NET_MODE_CLIENT;
            ctx->useServer = true;
            strncpy(ctx->serverIp, DEFAULT_RELAY_IP, sizeof(ctx->serverIp) - 1);
            ctx->serverIp[sizeof(ctx->serverIp) - 1] = '\0';
            ctx->serverPort = DEFAULT_RELAY_PORT;
            ctx->roomName[0] = '\0';
            ctx->password[0] = '\0';
            return SCENE_MATCHING;
        default:
            return SCENE_LOBBY;
    }
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
        int menuW = 32;
        int menuH = 4;
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
                return lobbySelect(ctx, selected);
            case 'q':
            case 'Q':
                return SCENE_QUIT;
            case '1': return lobbySelect(ctx, MENU_SINGLE);
            case '2': return lobbySelect(ctx, MENU_HOST);
            case '3': return lobbySelect(ctx, MENU_FIND);
            case '4': return lobbySelect(ctx, MENU_ONLINE);
        }

        usleep(16000);
    }
}
