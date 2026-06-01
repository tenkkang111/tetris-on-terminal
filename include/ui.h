#ifndef UI_H
#define UI_H

#include "common.h"
#include "network.h"
#include "augment.h"

typedef enum {
    UI_KEY_NONE = 0,
    UI_KEY_LEFT,
    UI_KEY_RIGHT,
    UI_KEY_SOFT_DROP,
    UI_KEY_ROTATE_CW,
    UI_KEY_ROTATE_CCW,
    UI_KEY_ROTATE_180,
    UI_KEY_HARD_DROP,
    UI_KEY_HOLD,
    UI_KEY_QUIT
} UiKey;

void uiInit(void);
void uiShutdown(void);

/**
 * @brief 화면 크기 검증. 너무 작으면 경고 표시 후 사용자 선택.
 * @param isMultiplayer 멀티플레이어 모드 여부 (더 넓은 화면 필요)
 * @return true 계속 진행, false 종료 요청
 */
bool uiCheckScreenSize(bool isMultiplayer);

/**
 * @brief 게임 중 화면 크기가 충분한지 검사 (논블로킹).
 * @param isMultiplayer 멀티플레이어 모드 여부
 * @return true 화면 충분, false 화면 부족 (일시정지 필요)
 */
bool uiIsScreenSizeOk(bool isMultiplayer);

/**
 * @brief 일시정지 오버레이 표시 (화면 크기 부족 시).
 * @param isMultiplayer 멀티플레이어 모드 여부
 * @param secondsLeft 게임 종료까지 남은 초 (-1이면 표시 안 함, 멀티플레이어 전용)
 */
void uiDrawPauseOverlay(bool isMultiplayer, int secondsLeft);

/**
 * @brief 상대 일시정지 대기 오버레이 표시.
 * @param secondsLeft 게임 종료까지 남은 초 (-1이면 표시 안 함)
 */
void uiDrawOpponentPauseOverlay(int secondsLeft);

UiKey uiPollKey(void);

/**
 * @brief 1프레임 렌더링.
 *        netCtx == NULL: 싱글, 아니면 듀얼.
 *        augInv / lvl: NULL이면 표시 생략.
 *        lockProgress: 0.0 = 착지 직후, 1.0 = 잠금 직전 (-1.0 = 미착지)
 */
void uiRender(const GameState* me, const NetContext* netCtx,
              const AugInventory* augInv, const LevelState* lvl,
              float lockProgress);

/**
 * @brief 카드 선택 모달. 마우스 클릭 또는 1/2/3 키로 선택.
 *        선택된 인덱스 반환 (0..2). Q 누르면 -1 (게임 종료 의도).
 *        모달 중에도 netPoll은 계속 호출되어 상대 메시지 큐가 쌓이지 않음.
 */
int  uiCardSelectModal(const GameState* me, NetContext* netCtx,
                       const AugInventory* augInv, const LevelState* lvl,
                       const CardOffer* offer);

void uiShowMessage(const char* msg);
void uiWaitKey(void);

#endif
