#ifndef UI_H
#define UI_H

#include "common.h"
#include "network.h"

typedef enum {
    UI_KEY_NONE = 0,
    UI_KEY_LEFT,
    UI_KEY_RIGHT,
    UI_KEY_DOWN,
    UI_KEY_ROTATE,
    UI_KEY_HARD_DROP,
    UI_KEY_HOLD,
    UI_KEY_QUIT
} UiKey;

void uiInit(void);
void uiShutdown(void);

/**
 * @brief 비차단 키 입력 1회 폴링.
 */
UiKey uiPollKey(void);

/**
 * @brief 1프레임 렌더링.
 *        netCtx == NULL: 싱글, 아니면 듀얼(본인 + 상대).
 */
void uiRender(const GameState* me, const NetContext* netCtx);

/**
 * @brief 화면 하단에 한 줄 메시지 출력 + refresh.
 */
void uiShowMessage(const char* msg);

/**
 * @brief 키 입력을 한 번 기다림 (게임 종료 화면용).
 */
void uiWaitKey(void);

#endif
