#include "network.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define MSG_HELLO  0x01
#define MSG_LOCK   0x02
#define MSG_OVER   0x03
#define MSG_STATE  0x04
#define MSG_HOLD   0x05
#define MSG_PAUSE  0x06
#define MSG_BOARD  0x07

#define BOARD_BYTES 200u
/* type + garbage + score(4) + board(200) + hold + pendingG + b2b + combo(2)
   + lines(4) + bagIdx + bag(7) + nextBag(7) = 230 */
#define LOCK_PAYLOAD  (1u + 1u + 4u + BOARD_BYTES + 1u + 1u + 1u + 2u + 4u + 1u + 7u + 7u)
#define STATE_PAYLOAD (1u + 1u + 1u + 1u + 1u)       /* type + type + rotation + x + y */
#define HOLD_PAYLOAD  (1u + 1u + 1u)                  /* type + hold + bagIdx */

static int sendAll(int sock, const void* buf, size_t len)
{
    const uint8_t* p = (const uint8_t*)buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = send(sock, p, left, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += (size_t)n;
        left -= (size_t)n;
    }
    return 0;
}

static int recvAll(int sock, void* buf, size_t len)
{
    uint8_t* p = (uint8_t*)buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = recv(sock, p, left, 0);
        if (n == 0) return -1;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += (size_t)n;
        left -= (size_t)n;
    }
    return 0;
}

static void writeU32LE(uint8_t* dst, uint32_t v)
{
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
    dst[2] = (uint8_t)((v >> 16) & 0xFF);
    dst[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t readU32LE(const uint8_t* src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

int netHost(NetContext* ctx, uint16_t port)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = NET_MODE_HOST;
    ctx->sock = -1;

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) return -1;

    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listenFd);
        return -1;
    }
    if (listen(listenFd, 1) < 0) {
        close(listenFd);
        return -1;
    }

    fprintf(stderr, "Waiting for opponent on port %u...\n", (unsigned)port);

    int sock = accept(listenFd, NULL, NULL);
    close(listenFd);
    if (sock < 0) return -1;

    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    ctx->sock = sock;
    ctx->connected = true;
    return 0;
}

int netJoin(NetContext* ctx, const char* host, uint16_t port)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = NET_MODE_CLIENT;
    ctx->sock = -1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    ctx->sock = sock;
    ctx->connected = true;
    return 0;
}

int netExchangeSeed(NetContext* ctx)
{
    if (!ctx->connected) return -1;

    uint8_t buf[5];
    if (ctx->mode == NET_MODE_HOST) {
        uint32_t seed = (uint32_t)time(NULL);
        if (seed == 0) seed = 1;
        buf[0] = MSG_HELLO;
        writeU32LE(buf + 1, seed);
        if (sendAll(ctx->sock, buf, sizeof(buf)) < 0) return -1;
        ctx->seed = seed;
    } else {
        if (recvAll(ctx->sock, buf, sizeof(buf)) < 0) return -1;
        if (buf[0] != MSG_HELLO) return -1;
        ctx->seed = readU32LE(buf + 1);
    }
    return 0;
}

int netSendLock(NetContext* ctx, const GameState* state, uint8_t garbageToSend)
{
    if (!ctx->connected) return 0;

    uint8_t buf[LOCK_PAYLOAD];
    size_t p = 0;
    buf[p++] = MSG_LOCK;
    buf[p++] = garbageToSend;
    writeU32LE(buf + p, state->score); p += 4;
    memcpy(buf + p, state->board, BOARD_BYTES); p += BOARD_BYTES;
    buf[p++] = (uint8_t)state->holdBlock;
    buf[p++] = state->pendingGarbage;
    buf[p++] = state->b2b;
    buf[p++] = (uint8_t)(state->combo & 0xFF);
    buf[p++] = (uint8_t)((state->combo >> 8) & 0xFF);
    writeU32LE(buf + p, state->totalLines); p += 4;
    buf[p++] = state->bagState.currentIndex;
    for (int i = 0; i < 7; i++) buf[p++] = (uint8_t)state->bagState.bag[i];
    for (int i = 0; i < 7; i++) buf[p++] = (uint8_t)state->bagState.nextBag[i];

    if (sendAll(ctx->sock, buf, sizeof(buf)) < 0) {
        ctx->connected = false;
        return -1;
    }
    return 0;
}

int netSendHold(NetContext* ctx, const GameState* state)
{
    if (!ctx->connected) return 0;
    uint8_t buf[HOLD_PAYLOAD];
    buf[0] = MSG_HOLD;
    buf[1] = (uint8_t)state->holdBlock;
    buf[2] = state->bagState.currentIndex;
    if (sendAll(ctx->sock, buf, sizeof(buf)) < 0) {
        ctx->connected = false;
        return -1;
    }
    return 0;
}

int netSendGameOver(NetContext* ctx)
{
    if (!ctx->connected) return 0;
    uint8_t b = MSG_OVER;
    if (sendAll(ctx->sock, &b, 1) < 0) {
        ctx->connected = false;
        return -1;
    }
    return 0;
}

int netSendPause(NetContext* ctx, bool paused)
{
    if (!ctx->connected) return 0;
    uint8_t buf[2];
    buf[0] = MSG_PAUSE;
    buf[1] = paused ? 1 : 0;
    if (sendAll(ctx->sock, buf, sizeof(buf)) < 0) {
        ctx->connected = false;
        return -1;
    }
    ctx->myPaused = paused;
    return 0;
}

int netSendBoardUpdate(NetContext* ctx, const GameState* state)
{
    if (!ctx->connected) return 0;

    /* type(1) + board(200) + score(4) + pendingGarbage(1) = 206 bytes */
    uint8_t buf[1 + BOARD_BYTES + 4 + 1];
    size_t p = 0;
    buf[p++] = MSG_BOARD;
    memcpy(buf + p, state->board, BOARD_BYTES); p += BOARD_BYTES;
    writeU32LE(buf + p, state->score); p += 4;
    buf[p++] = state->pendingGarbage;

    if (sendAll(ctx->sock, buf, sizeof(buf)) < 0) {
        ctx->connected = false;
        return -1;
    }
    return 0;
}

int netSendState(NetContext* ctx, const CurrentBlock* active)
{
    if (!ctx->connected) return 0;
    uint8_t buf[STATE_PAYLOAD];
    buf[0] = MSG_STATE;
    buf[1] = (uint8_t)active->type;
    buf[2] = active->rotation;
    buf[3] = (uint8_t)active->x;  /* int8_t를 그대로 바이트 인코딩 */
    buf[4] = (uint8_t)active->y;
    if (sendAll(ctx->sock, buf, sizeof(buf)) < 0) {
        ctx->connected = false;
        return -1;
    }
    return 0;
}

int netPoll(NetContext* ctx, GameState* myState)
{
    if (!ctx->connected) return -1;

    int handled = 0;
    /* 누적된 메시지를 모두 소진 (60Hz STATE가 쌓이지 않도록) */
    for (int safety = 0; safety < 256; safety++) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ctx->sock, &rfds);
        struct timeval tv = {0, 0};
        int r = select(ctx->sock + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) break;

        uint8_t header;
        ssize_t n = recv(ctx->sock, &header, 1, 0);
        if (n <= 0) {
            ctx->connected = false;
            return -1;
        }

        if (header == MSG_LOCK) {
            uint8_t rest[LOCK_PAYLOAD - 1];
            if (recvAll(ctx->sock, rest, sizeof(rest)) < 0) {
                ctx->connected = false;
                return -1;
            }
            size_t p = 0;
            uint8_t garbage = rest[p++];
            ctx->opponentScore = readU32LE(rest + p); p += 4;
            memcpy(ctx->opponentBoard, rest + p, BOARD_BYTES); p += BOARD_BYTES;
            ctx->opponentHold = (BlockType)rest[p++];
            ctx->opponentPendingGarbage = rest[p++];
            ctx->opponentB2b = rest[p++];
            ctx->opponentCombo = (uint16_t)rest[p] | ((uint16_t)rest[p + 1] << 8);
            p += 2;
            ctx->opponentTotalLines = readU32LE(rest + p); p += 4;
            ctx->opponentBag.currentIndex = rest[p++];
            for (int i = 0; i < 7; i++) ctx->opponentBag.bag[i] = (BlockType)rest[p++];
            for (int i = 0; i < 7; i++) ctx->opponentBag.nextBag[i] = (BlockType)rest[p++];

            if (garbage > 0) {
                uint16_t total = (uint16_t)ctx->incomingGarbageBuf + (uint16_t)garbage;
                if (total > 40) total = 40;
                ctx->incomingGarbageBuf = (uint8_t)total;
            }
            (void)myState;  /* shield 적용을 main이 담당하므로 여기서는 안 건드림 */

            /* lock 시점에 활성 블록은 이미 보드에 박혔으므로,
             * 다음 STATE가 올 때까지 상대 active 표시는 잠깐 숨김. */
            ctx->opponentHasActive = false;
            handled++;
            continue;
        }

        if (header == MSG_HOLD) {
            uint8_t rest[HOLD_PAYLOAD - 1];
            if (recvAll(ctx->sock, rest, sizeof(rest)) < 0) {
                ctx->connected = false;
                return -1;
            }
            ctx->opponentHold = (BlockType)rest[0];
            ctx->opponentBag.currentIndex = rest[1];
            handled++;
            continue;
        }

        if (header == MSG_STATE) {
            uint8_t rest[STATE_PAYLOAD - 1];
            if (recvAll(ctx->sock, rest, sizeof(rest)) < 0) {
                ctx->connected = false;
                return -1;
            }
            BlockType t = (BlockType)rest[0];
            uint8_t rot = rest[1];
            if (t >= I && t <= Z && rot <= 3) {
                ctx->opponentActive.type = t;
                ctx->opponentActive.rotation = rot;
                ctx->opponentActive.x = (int8_t)rest[2];
                ctx->opponentActive.y = (int8_t)rest[3];
                ctx->opponentHasActive = true;
            }
            handled++;
            continue;
        }

        if (header == MSG_OVER) {
            ctx->opponentLost = true;
            handled++;
            continue;
        }

        if (header == MSG_PAUSE) {
            uint8_t paused;
            if (recvAll(ctx->sock, &paused, 1) < 0) {
                ctx->connected = false;
                return -1;
            }
            ctx->opponentPaused = (paused != 0);
            if (ctx->opponentPaused) {
                /* 일시정지 시작 시간 기록 */
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                ctx->opponentPausedAt = (uint64_t)ts.tv_sec * 1000ULL +
                                        (uint64_t)ts.tv_nsec / 1000000ULL;
            }
            handled++;
            continue;
        }

        if (header == MSG_BOARD) {
            /* board(200) + score(4) + pendingGarbage(1) = 205 bytes */
            uint8_t rest[BOARD_BYTES + 4 + 1];
            if (recvAll(ctx->sock, rest, sizeof(rest)) < 0) {
                ctx->connected = false;
                return -1;
            }
            size_t p = 0;
            memcpy(ctx->opponentBoard, rest + p, BOARD_BYTES); p += BOARD_BYTES;
            ctx->opponentScore = readU32LE(rest + p); p += 4;
            ctx->opponentPendingGarbage = rest[p++];
            handled++;
            continue;
        }

        /* 알 수 없는 메시지: 연결 종료 */
        ctx->connected = false;
        return -1;
    }

    return handled;
}

void netClose(NetContext* ctx)
{
    if (ctx->connected && ctx->sock >= 0) {
        close(ctx->sock);
    }
    ctx->connected = false;
    ctx->sock = -1;
}

/* ============================================================================
 *  LAN 검색 (UDP 브로드캐스트)
 * ============================================================================ */

#include <ifaddrs.h>
#include <net/if.h>

int netBroadcastCreate(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    return sock;
}

int netBroadcastSend(int bcSock, uint16_t gamePort, const char* roomName, bool hasPassword)
{
    if (bcSock < 0) return -1;

    /* 패킷 포맷: "TETRIS1:포트:비밀번호유무:방이름" */
    char buf[96];
    int len = snprintf(buf, sizeof(buf), "%s:%u:%d:%s",
                       NET_BROADCAST_MAGIC, (unsigned)gamePort,
                       hasPassword ? 1 : 0,
                       roomName ? roomName : "");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NET_BROADCAST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    ssize_t sent = sendto(bcSock, buf, (size_t)len, 0,
                          (struct sockaddr*)&addr, sizeof(addr));
    return (sent > 0) ? 0 : -1;
}

void netBroadcastClose(int bcSock)
{
    if (bcSock >= 0) close(bcSock);
}

static uint64_t nowMsNet(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

int netDiscoverHosts(HostList* list, int timeoutMs)
{
    memset(list, 0, sizeof(*list));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 0;

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(NET_BROADCAST_PORT);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
        close(sock);
        return 0;
    }

    /* 논블로킹 설정 */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    uint64_t start = nowMsNet();
    uint64_t deadline = start + (uint64_t)timeoutMs;

    while (nowMsNet() < deadline) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);

        struct timeval tv;
        uint64_t remaining = deadline - nowMsNet();
        if (remaining > 100) remaining = 100;  /* 100ms 단위로 체크 */
        tv.tv_sec = 0;
        tv.tv_usec = (long)(remaining * 1000);

        int r = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) continue;

        char buf[128];
        struct sockaddr_in srcAddr;
        socklen_t srcLen = sizeof(srcAddr);
        ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr*)&srcAddr, &srcLen);
        if (n <= 0) continue;
        buf[n] = '\0';

        /* 패킷 검증: "TETRIS1:포트:비밀번호유무:방이름" */
        if (strncmp(buf, NET_BROADCAST_MAGIC ":", 8) != 0) continue;

        /* 포트 파싱 */
        char* portStr = buf + 8;
        char* colon1 = strchr(portStr, ':');
        uint16_t port;
        bool hasPassword = false;
        char roomName[32] = "";

        if (colon1 == NULL) {
            /* 이전 포맷: "TETRIS1:포트" (하위 호환) */
            port = (uint16_t)atoi(portStr);
        } else {
            /* 새 포맷: "TETRIS1:포트:비밀번호유무:방이름" */
            *colon1 = '\0';
            port = (uint16_t)atoi(portStr);

            char* pwStr = colon1 + 1;
            char* colon2 = strchr(pwStr, ':');
            if (colon2) {
                *colon2 = '\0';
                hasPassword = (atoi(pwStr) != 0);
                char* nameStr = colon2 + 1;
                strncpy(roomName, nameStr, sizeof(roomName) - 1);
                roomName[sizeof(roomName) - 1] = '\0';
            }
        }

        if (port == 0) continue;

        char ip[16];
        inet_ntop(AF_INET, &srcAddr.sin_addr, ip, sizeof(ip));

        /* 중복 체크 */
        bool found = false;
        for (int i = 0; i < list->count; i++) {
            if (strcmp(list->hosts[i].ip, ip) == 0 && list->hosts[i].port == port) {
                list->hosts[i].lastSeen = nowMsNet();
                /* 방 정보도 업데이트 */
                list->hosts[i].hasPassword = hasPassword;
                strncpy(list->hosts[i].roomName, roomName, sizeof(list->hosts[i].roomName) - 1);
                found = true;
                break;
            }
        }

        if (!found && list->count < NET_MAX_HOSTS) {
            strncpy(list->hosts[list->count].ip, ip, 15);
            list->hosts[list->count].ip[15] = '\0';
            list->hosts[list->count].port = port;
            list->hosts[list->count].hasPassword = hasPassword;
            strncpy(list->hosts[list->count].roomName, roomName, sizeof(list->hosts[list->count].roomName) - 1);
            list->hosts[list->count].roomName[sizeof(list->hosts[list->count].roomName) - 1] = '\0';
            list->hosts[list->count].lastSeen = nowMsNet();
            list->count++;
        }
    }

    close(sock);
    return list->count;
}

int netGetLocalIP(char* out)
{
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return -1;

    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        /* loopback 제외 */
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        /* 활성화된 인터페이스만 */
        if (!(ifa->ifa_flags & IFF_UP)) continue;

        struct sockaddr_in *addr = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, out, 16);

        /* 127.x.x.x 제외 */
        if (strncmp(out, "127.", 4) != 0) {
            found = 1;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return found ? 0 : -1;
}

#define MSG_PASSWORD 0x08
#define MSG_PASSWORD_OK 0x09
#define MSG_PASSWORD_FAIL 0x0A

int netSendPassword(NetContext* ctx, const char* password)
{
    if (!ctx->connected) return -1;

    /* 패킷: type(1) + len(1) + password(최대 31자) */
    uint8_t buf[33];
    size_t pwLen = password ? strlen(password) : 0;
    if (pwLen > 31) pwLen = 31;

    buf[0] = MSG_PASSWORD;
    buf[1] = (uint8_t)pwLen;
    if (pwLen > 0) memcpy(buf + 2, password, pwLen);

    if (sendAll(ctx->sock, buf, 2 + pwLen) < 0) {
        ctx->connected = false;
        return -1;
    }
    return 0;
}

int netReceiveAndVerifyPassword(NetContext* ctx, const char* expectedPassword)
{
    if (!ctx->connected) return -1;

    /* 비밀번호가 없으면 바로 OK */
    if (expectedPassword == NULL || expectedPassword[0] == '\0') {
        uint8_t ok = MSG_PASSWORD_OK;
        if (sendAll(ctx->sock, &ok, 1) < 0) {
            ctx->connected = false;
            return -1;
        }
        return 1;
    }

    /* 비밀번호 메시지 수신 */
    uint8_t header[2];
    if (recvAll(ctx->sock, header, 2) < 0) {
        ctx->connected = false;
        return -1;
    }

    if (header[0] != MSG_PASSWORD) {
        ctx->connected = false;
        return -1;
    }

    uint8_t pwLen = header[1];
    if (pwLen > 31) {
        ctx->connected = false;
        return -1;
    }

    char recvPassword[32] = "";
    if (pwLen > 0) {
        if (recvAll(ctx->sock, recvPassword, pwLen) < 0) {
            ctx->connected = false;
            return -1;
        }
    }
    recvPassword[pwLen] = '\0';

    /* 비밀번호 비교 */
    if (strcmp(recvPassword, expectedPassword) == 0) {
        uint8_t ok = MSG_PASSWORD_OK;
        if (sendAll(ctx->sock, &ok, 1) < 0) {
            ctx->connected = false;
            return -1;
        }
        return 1;
    } else {
        uint8_t fail = MSG_PASSWORD_FAIL;
        sendAll(ctx->sock, &fail, 1);
        return 0;
    }
}

int netReceivePasswordResult(NetContext* ctx)
{
    if (!ctx->connected) return -1;

    uint8_t result;
    if (recvAll(ctx->sock, &result, 1) < 0) {
        ctx->connected = false;
        return -1;
    }

    if (result == MSG_PASSWORD_OK) return 1;
    if (result == MSG_PASSWORD_FAIL) return 0;

    ctx->connected = false;
    return -1;
}

/* ============================================================================
 *  중계 서버 (server-client) 모드
 * ============================================================================ */

#define SRV_MSG_HELLO_LOCAL   0x10
#define SRV_MSG_MATCHED_LOCAL 0x18
#define SRV_MSG_FULL_LOCAL    0x19
#define SRV_HELLO_BYTES       34
#define SRV_ROOM_NAME_BYTES   32

int netConnectToServer(NetContext* ctx, const char* server, uint16_t port,
                       const char* roomName)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = NET_MODE_NONE;
    ctx->sock = -1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    uint8_t hello[SRV_HELLO_BYTES];
    memset(hello, 0, sizeof(hello));
    hello[0] = SRV_MSG_HELLO_LOCAL;
    hello[1] = 0x01;  /* protocol version */
    if (roomName && roomName[0]) {
        strncpy((char*)(hello + 2), roomName, SRV_ROOM_NAME_BYTES - 1);
    }
    if (sendAll(sock, hello, sizeof(hello)) < 0) {
        close(sock);
        return -1;
    }

    ctx->sock = sock;
    ctx->connected = true;
    /* mode는 매칭 후에 결정 */
    return 0;
}

int netCheckMatched(NetContext* ctx)
{
    if (!ctx->connected || ctx->sock < 0) return -1;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ctx->sock, &rfds);
    struct timeval tv = {0, 0};
    int r = select(ctx->sock + 1, &rfds, NULL, NULL, &tv);
    if (r <= 0) return 0;

    uint8_t hdr[2];
    ssize_t n = recv(ctx->sock, hdr, 1, MSG_PEEK);
    if (n <= 0) {
        ctx->connected = false;
        return -1;
    }

    /* FULL은 1바이트 */
    if (hdr[0] == SRV_MSG_FULL_LOCAL) {
        (void)recv(ctx->sock, hdr, 1, 0);
        ctx->connected = false;
        return -1;
    }
    if (hdr[0] != SRV_MSG_MATCHED_LOCAL) {
        ctx->connected = false;
        return -1;
    }

    /* MATCHED는 2바이트 모이길 기다림 */
    n = recv(ctx->sock, hdr, 2, MSG_PEEK);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        ctx->connected = false;
        return -1;
    }
    if (n < 2) return 0;

    if (recv(ctx->sock, hdr, 2, 0) != 2) {
        ctx->connected = false;
        return -1;
    }

    ctx->mode = (hdr[1] == 0) ? NET_MODE_HOST : NET_MODE_CLIENT;
    return 1;
}
