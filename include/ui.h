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

UiKey uiPollKey(void);

/**
 * @brief 1프레임 렌더링.
 *        netCtx == NULL: 싱글, 아니면 듀얼.
 *        augInv / lvl: NULL이면 표시 생략.
 */
void uiRender(const GameState* me, const NetContext* netCtx,
              const AugInventory* augInv, const LevelState* lvl);

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
