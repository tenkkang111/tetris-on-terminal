#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define MAX_CLIENTS  64
#define RELAY_BUF    2048

typedef enum {
    CS_HELLO_WAIT = 0,    /* 핸드셰이크 수신 중 */
    CS_WAITING,           /* HELLO 받았고 파트너 기다리는 중 */
    CS_MATCHED            /* 페어 형성됨, 릴레이 중 */
} ClientState;

typedef struct {
    int sock;
    ClientState state;
    int partnerIdx;
    char roomName[SRV_ROOM_NAME_LEN];

    /* HELLO 수신 누적 버퍼 */
    uint8_t  helloBuf[SRV_HELLO_SIZE];
    int      helloLen;

    /* 연결 정보 (로그용) */
    char     ipStr[INET_ADDRSTRLEN];
} SClient;

static SClient gClients[MAX_CLIENTS];
static volatile sig_atomic_t gShouldStop = 0;

static void onSigint(int sig) { (void)sig; gShouldStop = 1; }

static void slotInit(SClient* c)
{
    c->sock = -1;
    c->state = CS_HELLO_WAIT;
    c->partnerIdx = -1;
    c->helloLen = 0;
    c->roomName[0] = '\0';
    c->ipStr[0] = '\0';
}

static int slotFindFree(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (gClients[i].sock < 0) return i;
    }
    return -1;
}

static void slotClose(int idx)
{
    if (idx < 0 || idx >= MAX_CLIENTS) return;
    if (gClients[idx].sock >= 0) {
        close(gClients[idx].sock);
    }
    slotInit(&gClients[idx]);
}

/* 같은 roomName으로 WAITING 중인 슬롯 찾기. forIdx는 제외. */
static int slotFindWaiting(int forIdx, const char* roomName)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i == forIdx) continue;
        if (gClients[i].sock < 0) continue;
        if (gClients[i].state != CS_WAITING) continue;
        if (strncmp(gClients[i].roomName, roomName, SRV_ROOM_NAME_LEN) != 0) continue;
        return i;
    }
    return -1;
}

static int sendAll(int sock, const void* buf, size_t len)
{
    const uint8_t* p = (const uint8_t*)buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = send(sock, p, left, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += (size_t)n;
        left -= (size_t)n;
    }
    return 0;
}

static const char* describeRoom(const char* name)
{
    return (name[0] == '\0') ? "(quick)" : name;
}

int runServer(uint16_t port)
{
    /* SIGPIPE 무시: 닫힌 소켓에 send해도 프로세스가 죽지 않음 */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    sa.sa_handler = onSigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) slotInit(&gClients[i]);

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listenFd);
        return 1;
    }
    if (listen(listenFd, 16) < 0) {
        perror("listen");
        close(listenFd);
        return 1;
    }

    printf("TETRIS Relay Server\n");
    printf("  listening on 0.0.0.0:%u\n", (unsigned)port);
    printf("  max clients: %d\n", MAX_CLIENTS);
    printf("  Ctrl+C to stop.\n");
    fflush(stdout);

    while (!gShouldStop) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenFd, &rfds);
        int maxFd = listenFd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (gClients[i].sock >= 0) {
                FD_SET(gClients[i].sock, &rfds);
                if (gClients[i].sock > maxFd) maxFd = gClients[i].sock;
            }
        }

        struct timeval tv = {1, 0};
        int r = select(maxFd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (r == 0) continue;

        /* 신규 연결 */
        if (FD_ISSET(listenFd, &rfds)) {
            struct sockaddr_in caddr;
            socklen_t clen = sizeof(caddr);
            int sock = accept(listenFd, (struct sockaddr*)&caddr, &clen);
            if (sock >= 0) {
                int one = 1;
                setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

                int idx = slotFindFree();
                if (idx < 0) {
                    uint8_t msg = SRV_MSG_FULL;
                    (void)sendAll(sock, &msg, 1);
                    close(sock);
                    printf("[server] reject: server full\n");
                    fflush(stdout);
                } else {
                    gClients[idx].sock = sock;
                    gClients[idx].state = CS_HELLO_WAIT;
                    gClients[idx].partnerIdx = -1;
                    gClients[idx].helloLen = 0;
                    inet_ntop(AF_INET, &caddr.sin_addr,
                              gClients[idx].ipStr, sizeof(gClients[idx].ipStr));
                    printf("[server] client #%d connected from %s\n", idx, gClients[idx].ipStr);
                    fflush(stdout);
                }
            }
        }

        /* 클라이언트 처리 */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (gClients[i].sock < 0) continue;
            if (!FD_ISSET(gClients[i].sock, &rfds)) continue;

            if (gClients[i].state == CS_HELLO_WAIT) {
                int need = SRV_HELLO_SIZE - gClients[i].helloLen;
                ssize_t n = recv(gClients[i].sock,
                                 gClients[i].helloBuf + gClients[i].helloLen,
                                 (size_t)need, 0);
                if (n <= 0) {
                    printf("[server] #%d disconnect during hello\n", i);
                    fflush(stdout);
                    slotClose(i);
                    continue;
                }
                gClients[i].helloLen += (int)n;
                if (gClients[i].helloLen < SRV_HELLO_SIZE) continue;

                if (gClients[i].helloBuf[0] != SRV_MSG_HELLO) {
                    printf("[server] #%d bad hello byte 0x%02x\n",
                           i, gClients[i].helloBuf[0]);
                    fflush(stdout);
                    slotClose(i);
                    continue;
                }

                memcpy(gClients[i].roomName, gClients[i].helloBuf + 2, SRV_ROOM_NAME_LEN);
                gClients[i].roomName[SRV_ROOM_NAME_LEN - 1] = '\0';

                int match = slotFindWaiting(i, gClients[i].roomName);
                if (match >= 0) {
                    gClients[match].state = CS_MATCHED;
                    gClients[match].partnerIdx = i;
                    gClients[i].state = CS_MATCHED;
                    gClients[i].partnerIdx = match;

                    uint8_t mHost[2]   = { SRV_MSG_MATCHED, 0 };
                    uint8_t mClient[2] = { SRV_MSG_MATCHED, 1 };

                    /* 먼저 들어와 있던 쪽이 host (seed 생성 권한) */
                    int sh = sendAll(gClients[match].sock, mHost, 2);
                    int sc = sendAll(gClients[i].sock, mClient, 2);
                    if (sh < 0 || sc < 0) {
                        printf("[server] MATCHED send failed, dropping pair\n");
                        fflush(stdout);
                        slotClose(match);
                        slotClose(i);
                    } else {
                        printf("[server] matched #%d (host) <-> #%d (client) room='%s'\n",
                               match, i, describeRoom(gClients[match].roomName));
                        fflush(stdout);
                    }
                } else {
                    gClients[i].state = CS_WAITING;
                    printf("[server] #%d waiting in room='%s'\n",
                           i, describeRoom(gClients[i].roomName));
                    fflush(stdout);
                }
            }
            else if (gClients[i].state == CS_MATCHED) {
                uint8_t buf[RELAY_BUF];
                ssize_t n = recv(gClients[i].sock, buf, sizeof(buf), 0);
                if (n <= 0) {
                    int p = gClients[i].partnerIdx;
                    printf("[server] #%d disconnected; closing partner #%d\n", i, p);
                    fflush(stdout);
                    slotClose(i);
                    if (p >= 0) slotClose(p);
                    continue;
                }
                int p = gClients[i].partnerIdx;
                if (p >= 0 && gClients[p].sock >= 0) {
                    if (sendAll(gClients[p].sock, buf, (size_t)n) < 0) {
                        printf("[server] relay %d -> %d failed\n", i, p);
                        fflush(stdout);
                        slotClose(p);
                        slotClose(i);
                    }
                }
            }
            else { /* CS_WAITING — 예상치 못한 데이터는 disconnect로 처리 */
                uint8_t buf[64];
                ssize_t n = recv(gClients[i].sock, buf, sizeof(buf), 0);
                if (n <= 0) {
                    printf("[server] #%d disconnected while waiting\n", i);
                    fflush(stdout);
                    slotClose(i);
                }
                /* 데이터가 와도 무시 (스펙 외) */
            }
        }
    }

    printf("[server] shutting down\n");
    for (int i = 0; i < MAX_CLIENTS; i++) slotClose(i);
    close(listenFd);
    return 0;
}
