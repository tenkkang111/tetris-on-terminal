#ifndef SCENE_H
#define SCENE_H

#include "common.h"
#include "network.h"
#include "augment.h"

/* ============================================================================
 *  씬 타입 정의
 * ============================================================================ */

typedef enum {
    SCENE_LOBBY,        /* 메인 메뉴 */
    SCENE_MATCHING,     /* 매칭 대기 (호스트/조인) */
    SCENE_GAME,         /* 게임 플레이 */
    SCENE_RESULT,       /* 결과 화면 */
    SCENE_QUIT          /* 종료 */
} SceneType;

/* ============================================================================
 *  게임 결과 데이터
 * ============================================================================ */

typedef struct {
    bool isWin;
    bool isDisconnect;
    uint32_t myScore;
    uint32_t myLines;
    uint32_t myLevel;
    uint32_t oppScore;
    uint32_t oppLines;
    /* 싱글플레이 전용 */
    bool isSinglePlayer;
} GameResult;

/* ============================================================================
 *  씬 컨텍스트 (씬 간 데이터 전달)
 * ============================================================================ */

typedef struct {
    /* 네트워크 설정 */
    NetMode netMode;
    char hostIp[64];
    uint16_t port;
    char roomName[32];
    char password[32];
    bool cliMode;  /* CLI에서 직접 시작 시 true */

    /* 중계 서버 모드 */
    bool useServer;          /* true = 서버 경유 연결 */
    char serverIp[64];       /* 서버 IP */
    uint16_t serverPort;     /* 서버 포트 */

    /* 네트워크 컨텍스트 (매칭 -> 게임 전달) */
    NetContext net;
    uint32_t seed;

    /* 게임 결과 (게임 -> 결과 전달) */
    GameResult result;
} SceneContext;

/* ============================================================================
 *  씬 함수 선언
 * ============================================================================ */

/* 초기화 */
void sceneContextInit(SceneContext* ctx);

/* 각 씬 실행 (다음 씬 타입 반환) */
SceneType sceneLobby(SceneContext* ctx);
SceneType sceneMatching(SceneContext* ctx);
SceneType sceneGame(SceneContext* ctx);
SceneType sceneResult(SceneContext* ctx);

#endif
