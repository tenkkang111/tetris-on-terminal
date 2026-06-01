#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

#define NET_BROADCAST_PORT 5556
#define NET_BROADCAST_MAGIC "TETRIS1"
#define NET_MAX_HOSTS 8

typedef enum {
    NET_MODE_NONE = 0,
    NET_MODE_HOST,
    NET_MODE_CLIENT
} NetMode;

/* LAN 검색용 호스트 정보 */
typedef struct {
    char ip[16];
    uint16_t port;
    char roomName[32];
    bool hasPassword;
    uint64_t lastSeen;   /* ms timestamp */
} DiscoveredHost;

typedef struct {
    DiscoveredHost hosts[NET_MAX_HOSTS];
    int count;
} HostList;

/* 일시정지 타임아웃 (밀리초) */
#define NET_PAUSE_TIMEOUT_MS 60000

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

    /* 상대 풀 상태 (MSG_LOCK / MSG_HOLD 수신 시 갱신) */
    BlockType opponentHold;
    uint8_t   opponentPendingGarbage;
    uint8_t   opponentB2b;
    uint16_t  opponentCombo;
    uint32_t  opponentTotalLines;
    BagSystem opponentBag;   /* currentIndex / bag / nextBag 사용 (seed는 unused) */

    /* 수신 가비지 스테이징 — main에서 shield 처리 후 state.pendingGarbage로 이동 */
    uint8_t   incomingGarbageBuf;

    /* 일시정지 동기화 */
    bool      myPaused;           /* 내가 일시정지 중 */
    bool      opponentPaused;     /* 상대가 일시정지 중 */
    uint64_t  opponentPausedAt;   /* 상대 일시정지 시작 시간 (ms) */
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
 * @brief lockBlock 직후 호출. 본인 풀 스냅샷(board/score/hold/b2b/combo/lines/bag) + garbage 송신.
 */
int netSendLock(NetContext* ctx, const GameState* state, uint8_t garbageToSend);

/**
 * @brief Hold 액션 후 호출. hold 피스 + bag.currentIndex 즉시 동기화.
 */
int netSendHold(NetContext* ctx, const GameState* state);

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
 * @brief 일시정지 상태 변경 알림.
 * @param paused true=일시정지 시작, false=재개
 */
int netSendPause(NetContext* ctx, bool paused);

/**
 * @brief 보드 상태만 즉시 전송 (가비지 적용 후 동기화용).
 */
int netSendBoardUpdate(NetContext* ctx, const GameState* state);

/**
 * @brief 비차단 폴링. 수신된 LOCK 메시지의 garbage는 myState->pendingGarbage에 누적.
 * @return 1 메시지 처리됨, 0 없음, -1 연결 끊김.
 */
int netPoll(NetContext* ctx, GameState* myState);

void netClose(NetContext* ctx);

/* ============================================================================
 *  LAN 검색 (UDP 브로드캐스트)
 * ============================================================================ */

/**
 * @brief 브로드캐스트 소켓 생성 (호스트용).
 * @return 소켓 fd, 실패 시 -1.
 */
int netBroadcastCreate(void);

/**
 * @brief 호스트 광고 패킷 송신.
 * @param bcSock netBroadcastCreate()로 생성한 소켓
 * @param gamePort 게임 TCP 포트
 * @param roomName 방 이름 (최대 31자)
 * @param hasPassword 비밀번호 유무
 */
int netBroadcastSend(int bcSock, uint16_t gamePort, const char* roomName, bool hasPassword);

/**
 * @brief 브로드캐스트 소켓 닫기.
 */
void netBroadcastClose(int bcSock);

/**
 * @brief LAN 호스트 검색 (클라이언트용). timeoutMs 동안 수신.
 * @param list 발견된 호스트 목록 (out)
 * @param timeoutMs 검색 시간 (밀리초)
 * @return 발견된 호스트 수
 */
int netDiscoverHosts(HostList* list, int timeoutMs);

/**
 * @brief 호스트의 로컬 IP 주소 가져오기 (표시용).
 * @param out 결과 버퍼 (최소 16바이트)
 * @return 0 성공, -1 실패
 */
int netGetLocalIP(char* out);

/**
 * @brief 비밀번호 전송 (클라이언트 -> 호스트).
 */
int netSendPassword(NetContext* ctx, const char* password);

/**
 * @brief 비밀번호 수신 및 확인 (호스트).
 * @param ctx 네트워크 컨텍스트
 * @param expectedPassword 예상 비밀번호 (빈 문자열이면 비밀번호 없음)
 * @return 1 성공, 0 비밀번호 틀림, -1 연결 오류
 */
int netReceiveAndVerifyPassword(NetContext* ctx, const char* expectedPassword);

/**
 * @brief 비밀번호 검증 결과 수신 (클라이언트).
 * @return 1 성공, 0 비밀번호 틀림, -1 연결 오류
 */
int netReceivePasswordResult(NetContext* ctx);

#endif
