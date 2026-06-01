#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "scene.h"
#include "ui.h"
#include "server.h"

static void printHelp(const char* progName)
{
    printf("TETRIS Terminal Edition\n");
    printf("\n");
    printf("Usage: %s [command] [options]\n", progName);
    printf("\n");
    printf("Commands:\n");
    printf("  (none)                        Start with main menu\n");
    printf("  host [port] [name] [pass]     Host a game (LAN)\n");
    printf("  join <ip> [port] [pass]       Join a game by IP (LAN)\n");
    printf("  search                        Search LAN games and join\n");
    printf("  server [port]                 Run dedicated relay server (headless)\n");
    printf("  connect <ip> [port] [room]    Connect via relay server (cross-NAT)\n");
    printf("  help                          Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                            Start main menu\n", progName);
    printf("  %s host                       Host on default port (5555)\n", progName);
    printf("  %s host 6000                  Host on port 6000\n", progName);
    printf("  %s host 5555 \"My Room\"        Host with room name\n", progName);
    printf("  %s host 5555 \"My Room\" secret Host with password\n", progName);
    printf("  %s join 192.168.1.10          Join game at IP\n", progName);
    printf("  %s join 192.168.1.10 6000     Join with port\n", progName);
    printf("  %s join 192.168.1.10 5555 pw  Join with password\n", progName);
    printf("  %s search                     Search and select from LAN\n", progName);
    printf("\n");
    printf("Controls:\n");
    printf("  Arrow Keys    Move / Rotate (Up)\n");
    printf("  Space         Hard Drop\n");
    printf("  Z / X         Rotate CCW / CW\n");
    printf("  A             Rotate 180\n");
    printf("  C             Hold\n");
    printf("  Q             Quit\n");
    printf("\n");
}

int main(int argc, char* argv[])
{
    /* 서버 모드: ncurses 미초기화, 헤드리스로 실행 */
    if (argc >= 2 && strcmp(argv[1], "server") == 0) {
        uint16_t srvPort = 5555;
        if (argc >= 3) {
            int p = atoi(argv[2]);
            if (p > 0 && p <= 65535) srvPort = (uint16_t)p;
        }
        return runServer(srvPort);
    }

    SceneContext ctx;
    sceneContextInit(&ctx);

    SceneType startScene = SCENE_LOBBY;

    if (argc >= 2) {
        if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printHelp(argv[0]);
            return 0;
        }

        if (strcmp(argv[1], "host") == 0) {
            ctx.netMode = NET_MODE_HOST;
            ctx.port = 5555;
            ctx.roomName[0] = '\0';
            ctx.password[0] = '\0';
            ctx.cliMode = true;

            if (argc >= 3) {
                int port = atoi(argv[2]);
                if (port > 0 && port <= 65535) {
                    ctx.port = (uint16_t)port;
                }
            }
            if (argc >= 4) {
                strncpy(ctx.roomName, argv[3], sizeof(ctx.roomName) - 1);
                ctx.roomName[sizeof(ctx.roomName) - 1] = '\0';
            }
            if (argc >= 5) {
                strncpy(ctx.password, argv[4], sizeof(ctx.password) - 1);
                ctx.password[sizeof(ctx.password) - 1] = '\0';
            }

            startScene = SCENE_MATCHING;
        }
        else if (strcmp(argv[1], "join") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: IP address required\n");
                fprintf(stderr, "Usage: %s join <ip> [port] [password]\n", argv[0]);
                return 1;
            }

            ctx.netMode = NET_MODE_CLIENT;
            strncpy(ctx.hostIp, argv[2], sizeof(ctx.hostIp) - 1);
            ctx.hostIp[sizeof(ctx.hostIp) - 1] = '\0';
            ctx.port = 5555;
            ctx.password[0] = '\0';
            ctx.cliMode = true;

            if (argc >= 4) {
                int port = atoi(argv[3]);
                if (port > 0 && port <= 65535) {
                    ctx.port = (uint16_t)port;
                }
            }
            if (argc >= 5) {
                strncpy(ctx.password, argv[4], sizeof(ctx.password) - 1);
                ctx.password[sizeof(ctx.password) - 1] = '\0';
            }

            startScene = SCENE_MATCHING;
        }
        else if (strcmp(argv[1], "search") == 0) {
            ctx.netMode = NET_MODE_CLIENT;
            ctx.hostIp[0] = '\0';  /* 빈 hostIp = LAN 검색 모드 */
            ctx.port = 5555;
            ctx.password[0] = '\0';
            ctx.cliMode = true;

            startScene = SCENE_MATCHING;
        }
        else if (strcmp(argv[1], "connect") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: server IP required\n");
                fprintf(stderr, "Usage: %s connect <ip> [port] [room]\n", argv[0]);
                return 1;
            }
            ctx.netMode = NET_MODE_CLIENT;
            ctx.useServer = true;
            strncpy(ctx.serverIp, argv[2], sizeof(ctx.serverIp) - 1);
            ctx.serverIp[sizeof(ctx.serverIp) - 1] = '\0';
            ctx.serverPort = 5555;
            ctx.roomName[0] = '\0';   /* 빈 이름 = quick match */
            ctx.password[0] = '\0';
            ctx.cliMode = true;

            if (argc >= 4) {
                int p = atoi(argv[3]);
                if (p > 0 && p <= 65535) ctx.serverPort = (uint16_t)p;
            }
            if (argc >= 5) {
                strncpy(ctx.roomName, argv[4], sizeof(ctx.roomName) - 1);
                ctx.roomName[sizeof(ctx.roomName) - 1] = '\0';
            }

            startScene = SCENE_MATCHING;
        }
        else {
            fprintf(stderr, "Unknown command: %s\n", argv[1]);
            fprintf(stderr, "Use '%s help' for usage information\n", argv[0]);
            return 1;
        }
    }

    uiInit();

    SceneType currentScene = startScene;

    while (currentScene != SCENE_QUIT) {
        switch (currentScene) {
            case SCENE_LOBBY:
                currentScene = sceneLobby(&ctx);
                break;
            case SCENE_MATCHING:
                currentScene = sceneMatching(&ctx);
                break;
            case SCENE_GAME:
                currentScene = sceneGame(&ctx);
                break;
            case SCENE_RESULT:
                currentScene = sceneResult(&ctx);
                break;
            case SCENE_QUIT:
                break;
        }
    }

    uiShutdown();
    return 0;
}
