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
        int boxH = 12;
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

        fieldY += 3;
        if (has_colors()) attron(A_DIM);
        mvprintw(fieldY, boxX + 4, "TAB: Switch field  ENTER: Connect  ESC: Back");
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

static bool portInputScreen(SceneContext* ctx)
{
    char portBuf[INPUT_MAX + 1] = "5555";
    int portLen = (int)strlen(portBuf);

    nodelay(stdscr, TRUE);
    curs_set(1);

    while (1) {
        erase();

        int centerX = COLS / 2;
        int boxW = 40;
        int boxH = 8;
        int boxX = centerX - boxW / 2;
        int boxY = LINES / 2 - boxH / 2;

        if (has_colors()) attron(COLOR_PAIR(5));
        drawBox(boxY, boxX, boxW, boxH, "HOST GAME");
        if (has_colors()) attroff(COLOR_PAIR(5));

        int fieldY = boxY + 3;
        mvprintw(fieldY, boxX + 4, "Port:");
        attron(A_REVERSE);
        mvprintw(fieldY, boxX + 12, "%-10s", portBuf);
        attroff(A_REVERSE);

        fieldY += 2;
        if (has_colors()) attron(A_DIM);
        mvprintw(fieldY, boxX + 4, "ENTER: Start  ESC: Back");
        if (has_colors()) attroff(A_DIM);

        move(boxY + 3, boxX + 12 + portLen);
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
            int port = atoi(portBuf);
            if (port <= 0 || port > 65535) port = DEFAULT_PORT;
            ctx->port = (uint16_t)port;
            curs_set(0);
            return true;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (portLen > 0) {
                portBuf[--portLen] = '\0';
            }
        } else if (ch >= '0' && ch <= '9' && portLen < 5) {
            portBuf[portLen++] = (char)ch;
            portBuf[portLen] = '\0';
        }
    }
}

static bool waitingScreen(SceneContext* ctx, const char* status, bool isHost)
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
    int boxH = 10;
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
        mvprintw(boxY + 7, centerX - 10, "Port: %u", ctx->port);
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
    ctx->port = DEFAULT_PORT;
    memset(&ctx->net, 0, sizeof(ctx->net));
    ctx->net.sock = -1;

    if (ctx->netMode == NET_MODE_HOST) {
        if (!portInputScreen(ctx)) {
            return SCENE_LOBBY;
        }
    } else {
        if (!inputScreen(ctx)) {
            return SCENE_LOBBY;
        }
    }

    nodelay(stdscr, FALSE);
    curs_set(0);

    if (ctx->netMode == NET_MODE_HOST) {
        int listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) {
            waitingScreen(ctx, "Failed to create socket", true);
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
            waitingScreen(ctx, "Failed to bind port", true);
            usleep(2000000);
            return SCENE_LOBBY;
        }

        if (listen(listenFd, 1) < 0) {
            close(listenFd);
            waitingScreen(ctx, "Failed to listen", true);
            usleep(2000000);
            return SCENE_LOBBY;
        }

        int flags = fcntl(listenFd, F_GETFL, 0);
        fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);

        nodelay(stdscr, TRUE);

        while (1) {
            waitingScreen(ctx, "Waiting for opponent...", true);

            int ch = getch();
            if (ch == 27) {
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
                    close(listenFd);

                    int one = 1;
                    setsockopt(sock, IPPROTO_TCP, 0x01, &one, sizeof(one));

                    ctx->net.mode = NET_MODE_HOST;
                    ctx->net.sock = sock;
                    ctx->net.connected = true;

                    waitingScreen(ctx, "Player connected! Starting...", true);

                    if (netExchangeSeed(&ctx->net) != 0) {
                        netClose(&ctx->net);
                        waitingScreen(ctx, "Seed exchange failed", true);
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

        for (int attempt = 0; attempt < 100; attempt++) {
            char statusBuf[64];
            snprintf(statusBuf, sizeof(statusBuf), "Connecting... (attempt %d)", attempt + 1);
            waitingScreen(ctx, statusBuf, false);

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
                waitingScreen(ctx, "Invalid IP address", false);
                usleep(2000000);
                return SCENE_LOBBY;
            }

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                int one = 1;
                setsockopt(sock, IPPROTO_TCP, 0x01, &one, sizeof(one));

                ctx->net.mode = NET_MODE_CLIENT;
                ctx->net.sock = sock;
                ctx->net.connected = true;

                waitingScreen(ctx, "Connected! Exchanging seed...", false);

                if (netExchangeSeed(&ctx->net) != 0) {
                    netClose(&ctx->net);
                    waitingScreen(ctx, "Seed exchange failed", false);
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

        waitingScreen(ctx, "Connection failed", false);
        usleep(2000000);
        return SCENE_LOBBY;
    }
}
