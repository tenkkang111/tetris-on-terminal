#include "scene.h"
#include "ui.h"
#include "network.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define DEFAULT_PORT 5555
#define INPUT_MAX 63

typedef enum {
    FIELD_IP,
    FIELD_PORT,
    FIELD_COUNT
} InputField;

static void drawBox(int y, int x, int w, int h, const char* title)
{
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

    if (title) {
        int titleLen = (int)strlen(title);
        int titleX = x + (w - titleLen - 4) / 2;
        mvprintw(y, titleX, "[ %s ]", title);
    }
}

static const char* SPINNER = "|/-\\";

/* LAN 검색 결과 화면 - 호스트 목록 표시 및 선택 */
static int lanSearchScreen(SceneContext* ctx, HostList* hosts)
{
    int selected = 0;
    nodelay(stdscr, TRUE);
    curs_set(0);

    while (1) {
        erase();

        int centerX = COLS / 2;
        int boxW = 54;
        int boxH = 4 + hosts->count * 2 + 2;
        if (boxH < 10) boxH = 10;
        int boxX = centerX - boxW / 2;
        int boxY = LINES / 2 - boxH / 2;

        if (has_colors()) attron(COLOR_PAIR(2));
        drawBox(boxY, boxX, boxW, boxH, "LAN GAMES FOUND");
        if (has_colors()) attroff(COLOR_PAIR(2));

        if (hosts->count == 0) {
            mvprintw(boxY + 3, centerX - 8, "No games found");
            mvprintw(boxY + 5, centerX - 12, "Press R to retry, ESC to back");
        } else {
            int itemY = boxY + 2;
            for (int i = 0; i < hosts->count; i++) {
                if (i == selected) {
                    if (has_colors()) attron(COLOR_PAIR(2) | A_BOLD);
                    mvprintw(itemY, boxX + 3, "> ");
                } else {
                    mvprintw(itemY, boxX + 3, "  ");
                }

                /* 잠금 아이콘 */
                if (hosts->hosts[i].hasPassword) {
                    if (has_colors()) attron(COLOR_PAIR(3));
                    mvprintw(itemY, boxX + 5, "[P]");
                    if (has_colors()) attroff(COLOR_PAIR(3));
                    mvprintw(itemY, boxX + 8, " ");
                } else {
                    mvprintw(itemY, boxX + 5, "    ");
                }

                /* 방 이름 또는 IP */
                if (hosts->hosts[i].roomName[0] != '\0') {
                    mvprintw(itemY, boxX + 9, "%-20s", hosts->hosts[i].roomName);
                    if (has_colors()) attron(A_DIM);
                    mvprintw(itemY, boxX + 30, "(%s:%u)", hosts->hosts[i].ip, hosts->hosts[i].port);
                    if (has_colors()) attroff(A_DIM);
                } else {
                    mvprintw(itemY, boxX + 9, "%s:%u", hosts->hosts[i].ip, hosts->hosts[i].port);
                }

                if (i == selected && has_colors()) attroff(COLOR_PAIR(2) | A_BOLD);
                itemY += 2;
            }
            if (has_colors()) attron(A_DIM);
            mvprintw(boxY + boxH - 2, boxX + 4, "UP/DOWN: Select  ENTER: Connect  R: Retry");
            if (has_colors()) attroff(A_DIM);
        }

        refresh();

        int ch = getch();
        if (ch == ERR) {
            usleep(16000);
            continue;
        }

        if (ch == 27) return -1;  /* Back */
        if (ch == 'r' || ch == 'R') return -2;  /* Retry */

        if (hosts->count > 0) {
            if (ch == KEY_UP || ch == 'w' || ch == 'W') {
                selected = (selected - 1 + hosts->count) % hosts->count;
            } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
                selected = (selected + 1) % hosts->count;
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                strncpy(ctx->hostIp, hosts->hosts[selected].ip, sizeof(ctx->hostIp) - 1);
                ctx->port = hosts->hosts[selected].port;
                /* 선택한 호스트의 비밀번호 유무 저장 */
                if (hosts->hosts[selected].hasPassword) {
                    ctx->password[0] = '\x01';  /* 비밀번호 필요 마킹 */
                    ctx->password[1] = '\0';
                } else {
                    ctx->password[0] = '\0';
                }
                return selected;
            }
        }
    }
}

/* 비밀번호 입력 화면 */
static bool passwordInputScreen(SceneContext* ctx)
{
    char passBuf[32] = "";
    int passLen = 0;

    nodelay(stdscr, TRUE);
    curs_set(1);

    while (1) {
        erase();

        int centerX = COLS / 2;
        int boxW = 40;
        int boxH = 10;
        int boxX = centerX - boxW / 2;
        int boxY = LINES / 2 - boxH / 2;

        if (has_colors()) attron(COLOR_PAIR(3));
        drawBox(boxY, boxX, boxW, boxH, "PASSWORD REQUIRED");
        if (has_colors()) attroff(COLOR_PAIR(3));

        int fieldY = boxY + 3;
        mvprintw(fieldY, boxX + 4, "Password:");
        attron(A_REVERSE);
        if (passLen > 0) {
            char masked[32];
            memset(masked, '*', passLen);
            masked[passLen] = '\0';
            mvprintw(fieldY, boxX + 15, "%-16s", masked);
        } else {
            mvprintw(fieldY, boxX + 15, "                ");
        }
        attroff(A_REVERSE);

        fieldY += 3;
        if (has_colors()) attron(A_DIM);
        mvprintw(fieldY, boxX + 4, "ENTER: Connect  ESC: Back");
        if (has_colors()) attroff(A_DIM);

        move(boxY + 3, boxX + 15 + passLen);
        refresh();

        int ch = getch();
        if (ch == ERR) {
            usleep(16000);
            continue;
        }

        if (ch == 27) {
            curs_set(0);
            return false;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            strncpy(ctx->password, passBuf, sizeof(ctx->password) - 1);
            ctx->password[sizeof(ctx->password) - 1] = '\0';
            curs_set(0);
            return true;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (passLen > 0) {
                passBuf[--passLen] = '\0';
            }
        } else if (ch >= 32 && ch < 127 && passLen < 20) {
            passBuf[passLen++] = (char)ch;
            passBuf[passLen] = '\0';
        }
    }
}

/* 검색 진행 화면 */
static void searchingScreen(int progress)
{
    erase();

    int centerX = COLS / 2;
    int boxW = 40;
    int boxH = 8;
    int boxX = centerX - boxW / 2;
    int boxY = LINES / 2 - boxH / 2;

    if (has_colors()) attron(COLOR_PAIR(2));
    drawBox(boxY, boxX, boxW, boxH, "SEARCHING LAN");
    if (has_colors()) attroff(COLOR_PAIR(2));

    mvprintw(boxY + 3, centerX - 12, "Looking for games...");

    /* 프로그레스 바 */
    int barW = 20;
    int filled = (progress * barW) / 100;
    mvprintw(boxY + 5, centerX - barW / 2, "[");
    for (int i = 0; i < barW; i++) {
        if (i < filled) addch('=');
        else addch(' ');
    }
    addch(']');

    refresh();
}

static bool inputScreen(SceneContext* ctx)
{
    char ipBuf[INPUT_MAX + 1] = "127.0.0.1";
    char portBuf[INPUT_MAX + 1] = "5555";
    int ipLen = (int)strlen(ipBuf);
    int portLen = (int)strlen(portBuf);
    int activeField = FIELD_IP;

    nodelay(stdscr, TRUE);
    curs_set(1);

    while (1) {
        erase();

        int centerX = COLS / 2;
        int boxW = 50;
        int boxH = 14;
        int boxX = centerX - boxW / 2;
        int boxY = LINES / 2 - boxH / 2;

        if (has_colors()) attron(COLOR_PAIR(1));
        drawBox(boxY, boxX, boxW, boxH, "JOIN GAME");
        if (has_colors()) attroff(COLOR_PAIR(1));

        int fieldY = boxY + 3;
        mvprintw(fieldY, boxX + 4, "Server IP:");
        if (activeField == FIELD_IP) attron(A_REVERSE);
        mvprintw(fieldY, boxX + 16, "%-20s", ipBuf);
        if (activeField == FIELD_IP) attroff(A_REVERSE);

        fieldY += 2;
        mvprintw(fieldY, boxX + 4, "Port:");
        if (activeField == FIELD_PORT) attron(A_REVERSE);
        mvprintw(fieldY, boxX + 16, "%-10s", portBuf);
        if (activeField == FIELD_PORT) attroff(A_REVERSE);

        fieldY += 2;
        if (has_colors()) attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(fieldY, boxX + 4, "[F] Search LAN games");
        if (has_colors()) attroff(COLOR_PAIR(2) | A_BOLD);

        fieldY += 3;
        if (has_colors()) attron(A_DIM);
        mvprintw(fieldY, boxX + 4, "TAB: Switch  ENTER: Connect  F: Find  ESC: Back");
        if (has_colors()) attroff(A_DIM);

        if (activeField == FIELD_IP) {
            move(boxY + 3, boxX + 16 + ipLen);
        } else {
            move(boxY + 5, boxX + 16 + portLen);
        }

        refresh();

        int ch = getch();
        if (ch == ERR) {
            usleep(16000);
            continue;
        }

        if (ch == 27) {
            curs_set(0);
            return false;
        }

        /* LAN 검색 */
        if (ch == 'f' || ch == 'F') {
            curs_set(0);

lan_search_retry:
            searchingScreen(0);
            refresh();

            HostList hosts;
            memset(&hosts, 0, sizeof(hosts));

            /* 3초 동안 검색하면서 프로그레스 표시 */
            int totalMs = 3000;
            int stepMs = 100;
            for (int elapsed = 0; elapsed < totalMs; elapsed += stepMs) {
                searchingScreen((elapsed * 100) / totalMs);

                /* 짧은 검색 수행 */
                HostList partial;
                netDiscoverHosts(&partial, stepMs);
                for (int i = 0; i < partial.count && hosts.count < NET_MAX_HOSTS; i++) {
                    bool dup = false;
                    for (int j = 0; j < hosts.count; j++) {
                        if (strcmp(hosts.hosts[j].ip, partial.hosts[i].ip) == 0 &&
                            hosts.hosts[j].port == partial.hosts[i].port) {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup) hosts.hosts[hosts.count++] = partial.hosts[i];
                }
            }
            searchingScreen(100);

            int result = lanSearchScreen(ctx, &hosts);
            if (result == -1) {
                /* Back - 입력 화면으로 */
                curs_set(1);
                continue;
            } else if (result == -2) {
                /* Retry */
                goto lan_search_retry;
            } else {
                /* 선택됨 - 연결 진행 */
                return true;
            }
        }

        if (ch == '\t' || ch == KEY_DOWN || ch == KEY_UP) {
            activeField = (activeField + 1) % FIELD_COUNT;
            continue;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (ipLen == 0) continue;
            int port = atoi(portBuf);
            if (port <= 0 || port > 65535) port = DEFAULT_PORT;

            strncpy(ctx->hostIp, ipBuf, sizeof(ctx->hostIp) - 1);
            ctx->port = (uint16_t)port;
            curs_set(0);
            return true;
        }

        char* buf = (activeField == FIELD_IP) ? ipBuf : portBuf;
        int* len = (activeField == FIELD_IP) ? &ipLen : &portLen;
        int maxLen = (activeField == FIELD_IP) ? 20 : 10;

        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (*len > 0) {
                buf[--(*len)] = '\0';
            }
        } else if (ch >= 32 && ch < 127 && *len < maxLen) {
            buf[(*len)++] = (char)ch;
            buf[*len] = '\0';
        }
    }
}

typedef enum {
    SRV_FIELD_IP,
    SRV_FIELD_PORT,
    SRV_FIELD_ROOM,
    SRV_FIELD_COUNT
} ServerInputField;

/* 서버 연결용 입력 화면 (Server IP / Port / Room).
 * room이 비면 quick match. */
static bool serverInputScreen(SceneContext* ctx)
{
    char ipBuf[INPUT_MAX + 1]   = "";
    char portBuf[INPUT_MAX + 1] = "5555";
    char roomBuf[32]            = "";
    int ipLen   = (int)strlen(ipBuf);
    int portLen = (int)strlen(portBuf);
    int roomLen = (int)strlen(roomBuf);
    int activeField = SRV_FIELD_IP;

    nodelay(stdscr, TRUE);
    curs_set(1);

    while (1) {
        erase();

        int centerX = COLS / 2;
        int boxW = 52;
        int boxH = 16;
        int boxX = centerX - boxW / 2;
        int boxY = LINES / 2 - boxH / 2;

        if (has_colors()) attron(COLOR_PAIR(1));
        drawBox(boxY, boxX, boxW, boxH, "ONLINE GAME");
        if (has_colors()) attroff(COLOR_PAIR(1));

        int fieldY = boxY + 2;
        if (has_colors()) attron(A_DIM);
        mvprintw(fieldY, boxX + 4, "Connect via relay server.");
        if (has_colors()) attroff(A_DIM);

        fieldY = boxY + 4;
        mvprintw(fieldY, boxX + 4, "Server IP:");
        if (activeField == SRV_FIELD_IP) attron(A_REVERSE);
        mvprintw(fieldY, boxX + 17, "%-28s", ipBuf);
        if (activeField == SRV_FIELD_IP) attroff(A_REVERSE);

        fieldY += 2;
        mvprintw(fieldY, boxX + 4, "Port:");
        if (activeField == SRV_FIELD_PORT) attron(A_REVERSE);
        mvprintw(fieldY, boxX + 17, "%-10s", portBuf);
        if (activeField == SRV_FIELD_PORT) attroff(A_REVERSE);

        fieldY += 2;
        mvprintw(fieldY, boxX + 4, "Room:");
        if (activeField == SRV_FIELD_ROOM) attron(A_REVERSE);
        if (roomLen > 0) {
            mvprintw(fieldY, boxX + 17, "%-28s", roomBuf);
        } else {
            if (has_colors()) attron(A_DIM);
            mvprintw(fieldY, boxX + 17, "(quick match)               ");
            if (has_colors()) attroff(A_DIM);
        }
        if (activeField == SRV_FIELD_ROOM) attroff(A_REVERSE);

        fieldY += 3;
        if (has_colors()) attron(A_DIM);
        mvprintw(fieldY, boxX + 4, "TAB: Switch   ENTER: Connect   ESC: Back");
        if (has_colors()) attroff(A_DIM);

        if (activeField == SRV_FIELD_IP)        move(boxY + 4,  boxX + 17 + ipLen);
        else if (activeField == SRV_FIELD_PORT) move(boxY + 6,  boxX + 17 + portLen);
        else                                     move(boxY + 8,  boxX + 17 + roomLen);

        refresh();

        int ch = getch();
        if (ch == ERR) { usleep(16000); continue; }

        if (ch == 27) {
            curs_set(0);
            return false;
        }

        if (ch == '\t' || ch == KEY_DOWN) {
            activeField = (activeField + 1) % SRV_FIELD_COUNT;
            continue;
        }
        if (ch == KEY_UP) {
            activeField = (activeField - 1 + SRV_FIELD_COUNT) % SRV_FIELD_COUNT;
            continue;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (ipLen == 0) continue;   /* IP 필수 */
            int port = atoi(portBuf);
            if (port <= 0 || port > 65535) port = DEFAULT_PORT;

            strncpy(ctx->serverIp, ipBuf, sizeof(ctx->serverIp) - 1);
            ctx->serverIp[sizeof(ctx->serverIp) - 1] = '\0';
            ctx->serverPort = (uint16_t)port;
            strncpy(ctx->roomName, roomBuf, sizeof(ctx->roomName) - 1);
            ctx->roomName[sizeof(ctx->roomName) - 1] = '\0';
            curs_set(0);
            return true;
        }

        char* buf;
        int*  len;
        int   maxLen;
        if (activeField == SRV_FIELD_IP)        { buf = ipBuf;   len = &ipLen;   maxLen = 28; }
        else if (activeField == SRV_FIELD_PORT) { buf = portBuf; len = &portLen; maxLen = 5;  }
        else                                     { buf = roomBuf; len = &roomLen; maxLen = 28; }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (*len > 0) buf[--(*len)] = '\0';
        } else if (ch >= 32 && ch < 127 && *len < maxLen) {
            if (activeField == SRV_FIELD_PORT) {
                if (ch >= '0' && ch <= '9') { buf[(*len)++] = (char)ch; buf[*len] = '\0'; }
            } else {
                buf[(*len)++] = (char)ch; buf[*len] = '\0';
            }
        }
    }
}

typedef enum {
    HOST_FIELD_NAME,
    HOST_FIELD_PORT,
    HOST_FIELD_PASSWORD,
    HOST_FIELD_COUNT
} HostInputField;

static bool portInputScreen(SceneContext* ctx)
{
    char nameBuf[32] = "Game Room";
    char portBuf[INPUT_MAX + 1] = "5555";
    char passBuf[32] = "";
    int nameLen = (int)strlen(nameBuf);
    int portLen = (int)strlen(portBuf);
    int passLen = 0;
    int activeField = HOST_FIELD_NAME;

    nodelay(stdscr, TRUE);
    curs_set(1);

    while (1) {
        erase();

        int centerX = COLS / 2;
        int boxW = 46;
        int boxH = 14;
        int boxX = centerX - boxW / 2;
        int boxY = LINES / 2 - boxH / 2;

        if (has_colors()) attron(COLOR_PAIR(5));
        drawBox(boxY, boxX, boxW, boxH, "HOST GAME");
        if (has_colors()) attroff(COLOR_PAIR(5));

        int fieldY = boxY + 3;

        /* Room Name */
        mvprintw(fieldY, boxX + 4, "Room Name:");
        if (activeField == HOST_FIELD_NAME) attron(A_REVERSE);
        mvprintw(fieldY, boxX + 16, "%-24s", nameBuf);
        if (activeField == HOST_FIELD_NAME) attroff(A_REVERSE);

        fieldY += 2;

        /* Port */
        mvprintw(fieldY, boxX + 4, "Port:");
        if (activeField == HOST_FIELD_PORT) attron(A_REVERSE);
        mvprintw(fieldY, boxX + 16, "%-10s", portBuf);
        if (activeField == HOST_FIELD_PORT) attroff(A_REVERSE);

        fieldY += 2;

        /* Password */
        mvprintw(fieldY, boxX + 4, "Password:");
        if (activeField == HOST_FIELD_PASSWORD) attron(A_REVERSE);
        if (passLen > 0) {
            /* 비밀번호 마스킹 */
            char masked[32];
            memset(masked, '*', passLen);
            masked[passLen] = '\0';
            mvprintw(fieldY, boxX + 16, "%-20s", masked);
        } else {
            if (has_colors()) attron(A_DIM);
            mvprintw(fieldY, boxX + 16, "(none)              ");
            if (has_colors()) attroff(A_DIM);
        }
        if (activeField == HOST_FIELD_PASSWORD) attroff(A_REVERSE);

        fieldY += 2;
        if (has_colors()) attron(A_DIM);
        mvprintw(fieldY, boxX + 4, "TAB: Switch  ENTER: Start  ESC: Back");
        if (has_colors()) attroff(A_DIM);

        /* 커서 위치 */
        if (activeField == HOST_FIELD_NAME) {
            move(boxY + 3, boxX + 16 + nameLen);
        } else if (activeField == HOST_FIELD_PORT) {
            move(boxY + 5, boxX + 16 + portLen);
        } else {
            move(boxY + 7, boxX + 16 + passLen);
        }
        refresh();

        int ch = getch();
        if (ch == ERR) {
            usleep(16000);
            continue;
        }

        if (ch == 27) {
            curs_set(0);
            return false;
        }

        if (ch == '\t' || ch == KEY_DOWN) {
            activeField = (activeField + 1) % HOST_FIELD_COUNT;
            continue;
        }
        if (ch == KEY_UP) {
            activeField = (activeField - 1 + HOST_FIELD_COUNT) % HOST_FIELD_COUNT;
            continue;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            int port = atoi(portBuf);
            if (port <= 0 || port > 65535) port = DEFAULT_PORT;
            ctx->port = (uint16_t)port;
            strncpy(ctx->roomName, nameBuf, sizeof(ctx->roomName) - 1);
            ctx->roomName[sizeof(ctx->roomName) - 1] = '\0';
            strncpy(ctx->password, passBuf, sizeof(ctx->password) - 1);
            ctx->password[sizeof(ctx->password) - 1] = '\0';
            curs_set(0);
            return true;
        }

        /* 필드별 입력 처리 */
        char* buf;
        int* len;
        int maxLen;

        if (activeField == HOST_FIELD_NAME) {
            buf = nameBuf;
            len = &nameLen;
            maxLen = 24;
        } else if (activeField == HOST_FIELD_PORT) {
            buf = portBuf;
            len = &portLen;
            maxLen = 5;
        } else {
            buf = passBuf;
            len = &passLen;
            maxLen = 20;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (*len > 0) {
                buf[--(*len)] = '\0';
            }
        } else if (ch >= 32 && ch < 127 && *len < maxLen) {
            /* 포트는 숫자만 */
            if (activeField == HOST_FIELD_PORT) {
                if (ch >= '0' && ch <= '9') {
                    buf[(*len)++] = (char)ch;
                    buf[*len] = '\0';
                }
            } else {
                buf[(*len)++] = (char)ch;
                buf[*len] = '\0';
            }
        }
    }
}

static bool waitingScreen(SceneContext* ctx, const char* status, bool isHost, const char* localIP)
{
    static int spinIdx = 0;
    static uint64_t lastSpin = 0;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;

    if (now - lastSpin > 100) {
        spinIdx = (spinIdx + 1) % 4;
        lastSpin = now;
    }

    erase();

    int centerX = COLS / 2;
    int boxW = 50;
    int boxH = isHost ? 12 : 10;
    int boxX = centerX - boxW / 2;
    int boxY = LINES / 2 - boxH / 2;

    const char* title = isHost ? "HOSTING" : "CONNECTING";
    int colorPair = isHost ? 5 : 1;

    if (has_colors()) attron(COLOR_PAIR(colorPair));
    drawBox(boxY, boxX, boxW, boxH, title);
    if (has_colors()) attroff(COLOR_PAIR(colorPair));

    int statusLen = (int)strlen(status);
    mvprintw(boxY + 3, centerX - statusLen / 2, "%s", status);

    if (has_colors()) attron(COLOR_PAIR(colorPair) | A_BOLD);
    mvprintw(boxY + 5, centerX - 1, " %c ", SPINNER[spinIdx]);
    if (has_colors()) attroff(COLOR_PAIR(colorPair) | A_BOLD);

    if (isHost) {
        if (localIP && localIP[0]) {
            mvprintw(boxY + 7, centerX - 12, "LAN IP: %s", localIP);
        }
        mvprintw(boxY + 8, centerX - 8, "Port: %u", ctx->port);
        if (has_colors()) attron(A_DIM);
        mvprintw(boxY + 9, centerX - 18, "(LAN players can find you automatically)");
        if (has_colors()) attroff(A_DIM);
    } else {
        mvprintw(boxY + 7, centerX - 15, "%s:%u", ctx->hostIp, ctx->port);
    }

    if (has_colors()) attron(A_DIM);
    mvprintw(boxY + boxH + 1, centerX - 10, "Press ESC to cancel");
    if (has_colors()) attroff(A_DIM);

    refresh();
    return true;
}

SceneType sceneMatching(SceneContext* ctx)
{
    /* CLI 모드가 아닐 때만 포트 초기화 */
    if (!ctx->cliMode) {
        ctx->port = DEFAULT_PORT;
    }
    memset(&ctx->net, 0, sizeof(ctx->net));
    ctx->net.sock = -1;

    /* ---- 중계 서버 경유 모드 ---- */
    if (ctx->useServer) {
        /* CLI 모드가 아니고 IP가 비어있으면 입력 화면 */
        if (!ctx->cliMode && ctx->serverIp[0] == '\0') {
            if (!serverInputScreen(ctx)) {
                ctx->useServer = false;
                return SCENE_LOBBY;
            }
        }

        nodelay(stdscr, TRUE);
        curs_set(0);

        char status[160];
        snprintf(status, sizeof(status), "Connecting to server %s:%u...",
                 ctx->serverIp, ctx->serverPort);
        waitingScreen(ctx, status, false, NULL);

        if (netConnectToServer(&ctx->net, ctx->serverIp, ctx->serverPort,
                               ctx->roomName) != 0) {
            waitingScreen(ctx, "Failed to reach server", false, NULL);
            usleep(2000000);
            return SCENE_LOBBY;
        }

        /* 매칭 폴링 (ESC로 취소) */
        const char* waitMsg = (ctx->roomName[0])
            ? "Waiting for opponent in your room..."
            : "Waiting for quick match...";

        while (1) {
            waitingScreen(ctx, waitMsg, false, NULL);

            int ch = getch();
            if (ch == 27) {
                netClose(&ctx->net);
                return SCENE_LOBBY;
            }

            int r = netCheckMatched(&ctx->net);
            if (r == 1) break;
            if (r < 0) {
                waitingScreen(ctx, "Server closed connection", false, NULL);
                usleep(2000000);
                netClose(&ctx->net);
                return SCENE_LOBBY;
            }
            usleep(50000);
        }

        const char* roleMsg = (ctx->net.mode == NET_MODE_HOST)
            ? "Matched! You are HOST. Exchanging seed..."
            : "Matched! You are CLIENT. Receiving seed...";
        waitingScreen(ctx, roleMsg, false, NULL);

        /* netMode를 매칭 결과에 맞춰 갱신 */
        ctx->netMode = ctx->net.mode;

        if (netExchangeSeed(&ctx->net) != 0) {
            netClose(&ctx->net);
            waitingScreen(ctx, "Seed exchange failed", false, NULL);
            usleep(2000000);
            return SCENE_LOBBY;
        }

        ctx->seed = ctx->net.seed;
        usleep(400000);
        return SCENE_GAME;
    }

    if (ctx->netMode == NET_MODE_HOST) {
        /* CLI 모드면 입력 화면 건너뛰기 */
        if (!ctx->cliMode) {
            if (!portInputScreen(ctx)) {
                return SCENE_LOBBY;
            }
        }
    } else {
        /* FIND GAME 모드: hostIp가 비어있으면 바로 LAN 검색 시작 */
        if (ctx->hostIp[0] == '\0') {
            curs_set(0);
            nodelay(stdscr, TRUE);

lan_find_retry:
            searchingScreen(0);
            refresh();

            HostList hosts;
            memset(&hosts, 0, sizeof(hosts));

            int totalMs = 3000;
            int stepMs = 100;
            for (int elapsed = 0; elapsed < totalMs; elapsed += stepMs) {
                searchingScreen((elapsed * 100) / totalMs);

                HostList partial;
                netDiscoverHosts(&partial, stepMs);
                for (int i = 0; i < partial.count && hosts.count < NET_MAX_HOSTS; i++) {
                    bool dup = false;
                    for (int j = 0; j < hosts.count; j++) {
                        if (strcmp(hosts.hosts[j].ip, partial.hosts[i].ip) == 0 &&
                            hosts.hosts[j].port == partial.hosts[i].port) {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup) hosts.hosts[hosts.count++] = partial.hosts[i];
                }
            }
            searchingScreen(100);

            int result = lanSearchScreen(ctx, &hosts);
            if (result == -1) {
                return SCENE_LOBBY;
            } else if (result == -2) {
                goto lan_find_retry;
            }
            /* 호스트 선택됨 - 비밀번호가 필요하면 입력받기 */
            if (ctx->password[0] == '\x01') {
                if (!passwordInputScreen(ctx)) {
                    goto lan_find_retry;
                }
            }
        } else {
            /* CLI 모드면 입력 화면 건너뛰기 (join 명령) */
            if (!ctx->cliMode) {
                if (!inputScreen(ctx)) {
                    return SCENE_LOBBY;
                }
            }
        }
    }

    nodelay(stdscr, FALSE);
    curs_set(0);

    if (ctx->netMode == NET_MODE_HOST) {
        int listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) {
            waitingScreen(ctx, "Failed to create socket", true, NULL);
            usleep(2000000);
            return SCENE_LOBBY;
        }

        int yes = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(ctx->port);

        if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(listenFd);
            waitingScreen(ctx, "Failed to bind port", true, NULL);
            usleep(2000000);
            return SCENE_LOBBY;
        }

        if (listen(listenFd, 1) < 0) {
            close(listenFd);
            waitingScreen(ctx, "Failed to listen", true, NULL);
            usleep(2000000);
            return SCENE_LOBBY;
        }

        int flags = fcntl(listenFd, F_GETFL, 0);
        fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);

        /* LAN IP 가져오기 */
        char localIP[16] = "";
        netGetLocalIP(localIP);

        /* 브로드캐스트 소켓 생성 */
        int bcSock = netBroadcastCreate();
        uint64_t lastBroadcast = 0;

        nodelay(stdscr, TRUE);

        while (1) {
            waitingScreen(ctx, "Waiting for opponent...", true, localIP);

            /* 1초마다 브로드캐스트 송신 */
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint64_t now = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
            if (now - lastBroadcast >= 1000) {
                bool hasPassword = (ctx->password[0] != '\0');
                netBroadcastSend(bcSock, ctx->port, ctx->roomName, hasPassword);
                lastBroadcast = now;
            }

            int ch = getch();
            if (ch == 27) {
                netBroadcastClose(bcSock);
                close(listenFd);
                return SCENE_LOBBY;
            }

            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(listenFd, &rfds);
            struct timeval tv = {0, 50000};

            if (select(listenFd + 1, &rfds, NULL, NULL, &tv) > 0) {
                int sock = accept(listenFd, NULL, NULL);
                if (sock >= 0) {
                    int one = 1;
                    setsockopt(sock, IPPROTO_TCP, 0x01, &one, sizeof(one));

                    ctx->net.mode = NET_MODE_HOST;
                    ctx->net.sock = sock;
                    ctx->net.connected = true;

                    /* 비밀번호 확인 */
                    if (ctx->password[0] != '\0') {
                        waitingScreen(ctx, "Verifying password...", true, localIP);
                        int pwResult = netReceiveAndVerifyPassword(&ctx->net, ctx->password);
                        if (pwResult != 1) {
                            netClose(&ctx->net);
                            waitingScreen(ctx, pwResult == 0 ? "Wrong password - waiting..." : "Connection error - waiting...", true, localIP);
                            usleep(1500000);
                            /* 방을 유지하고 다음 연결 대기 */
                            continue;
                        }
                    }

                    /* 연결 성공 - 브로드캐스트 및 리슨 소켓 정리 */
                    netBroadcastClose(bcSock);
                    close(listenFd);

                    waitingScreen(ctx, "Player connected! Starting...", true, localIP);

                    if (netExchangeSeed(&ctx->net) != 0) {
                        netClose(&ctx->net);
                        waitingScreen(ctx, "Seed exchange failed", true, NULL);
                        usleep(2000000);
                        return SCENE_LOBBY;
                    }

                    ctx->seed = ctx->net.seed;
                    usleep(500000);
                    return SCENE_GAME;
                }
            }

            usleep(16000);
        }
    } else {
        nodelay(stdscr, TRUE);
        bool needsPassword = (ctx->password[0] != '\0');

client_connect_retry:
        for (int attempt = 0; attempt < 100; attempt++) {
            char statusBuf[64];
            snprintf(statusBuf, sizeof(statusBuf), "Connecting... (attempt %d)", attempt + 1);
            waitingScreen(ctx, statusBuf, false, NULL);

            int ch = getch();
            if (ch == 27) {
                return SCENE_LOBBY;
            }

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) continue;

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(ctx->port);

            if (inet_pton(AF_INET, ctx->hostIp, &addr.sin_addr) != 1) {
                close(sock);
                waitingScreen(ctx, "Invalid IP address", false, NULL);
                usleep(2000000);
                return SCENE_LOBBY;
            }

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                int one = 1;
                setsockopt(sock, IPPROTO_TCP, 0x01, &one, sizeof(one));

                ctx->net.mode = NET_MODE_CLIENT;
                ctx->net.sock = sock;
                ctx->net.connected = true;

                /* 비밀번호가 필요한 경우 전송 */
                if (needsPassword) {
                    waitingScreen(ctx, "Sending password...", false, NULL);
                    if (netSendPassword(&ctx->net, ctx->password) != 0) {
                        netClose(&ctx->net);
                        waitingScreen(ctx, "Connection error", false, NULL);
                        usleep(2000000);
                        return SCENE_LOBBY;
                    }

                    int pwResult = netReceivePasswordResult(&ctx->net);
                    if (pwResult == 0) {
                        /* 비밀번호 틀림 - 재입력 */
                        netClose(&ctx->net);
                        waitingScreen(ctx, "Wrong password - try again", false, NULL);
                        usleep(1500000);
                        if (!passwordInputScreen(ctx)) {
                            return SCENE_LOBBY;
                        }
                        goto client_connect_retry;
                    } else if (pwResult != 1) {
                        netClose(&ctx->net);
                        waitingScreen(ctx, "Connection error", false, NULL);
                        usleep(2000000);
                        return SCENE_LOBBY;
                    }
                }

                waitingScreen(ctx, "Connected! Exchanging seed...", false, NULL);

                if (netExchangeSeed(&ctx->net) != 0) {
                    netClose(&ctx->net);
                    waitingScreen(ctx, "Seed exchange failed", false, NULL);
                    usleep(2000000);
                    return SCENE_LOBBY;
                }

                ctx->seed = ctx->net.seed;
                usleep(500000);
                return SCENE_GAME;
            }

            close(sock);
            usleep(500000);
        }

        waitingScreen(ctx, "Connection failed", false, NULL);
        usleep(2000000);
        return SCENE_LOBBY;
    }
}
