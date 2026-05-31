#include "network.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define MSG_HELLO 0x01
#define MSG_LOCK  0x02
#define MSG_OVER  0x03
#define MSG_STATE 0x04
#define MSG_HOLD  0x05

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
