#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

typedef enum {
    NET_MODE_NONE = 0,
    NET_MODE_HOST,
    NET_MODE_CLIENT
} NetMode;

typedef struct {
    NetMode mode;
    int sock;
    uint32_t seed;
    bool connected;
    bool opponentLost;

    /* 상대 화면 표시용 스냅샷 (lockBlock 이벤트 도착 시 갱신) */
    uint8_t opponentBoard[20][10];
    uint32_t opponentScore;

    /* 상대 활성(낙하 중) 블록. MSG_STATE 수신 시 갱신 */
    CurrentBlock opponentActive;
    bool opponentHasActive;
} NetContext;

/**
 * @brief 호스트 모드: 지정 포트에서 1명 accept.
 * @return 0 on success, -1 on failure.
 */
int netHost(NetContext* ctx, uint16_t port);

/**
 * @brief 클라이언트 모드: host:port로 connect.
 */
int netJoin(NetContext* ctx, const char* host, uint16_t port);

/**
 * @brief 게임 시작 시 시드 교환.
 *        호스트가 시드 생성/송신, 클라이언트는 수신. 양쪽 모두 ctx->seed에 동일 값 저장.
 */
int netExchangeSeed(NetContext* ctx);

/**
 * @brief lockBlock 직후 호출. 본인 보드/점수 스냅샷 + 상대에게 보낼 garbage 줄 수 송신.
 */
int netSendLock(NetContext* ctx, const GameState* state, uint8_t garbageToSend);

/**
 * @brief 활성 블록 상태(타입/회전/x/y) 송신. 매 틱 호출 권장.
 *        상대 화면에 떨어지는 블록을 실시간 표시하기 위함.
 */
int netSendState(NetContext* ctx, const CurrentBlock* active);

/**
 * @brief 게임 오버 알림 송신.
 */
int netSendGameOver(NetContext* ctx);

/**
 * @brief 비차단 폴링. 수신된 LOCK 메시지의 garbage는 myState->pendingGarbage에 누적.
 * @return 1 메시지 처리됨, 0 없음, -1 연결 끊김.
 */
int netPoll(NetContext* ctx, GameState* myState);

void netClose(NetContext* ctx);

#endif
