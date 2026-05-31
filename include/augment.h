#ifndef AUGMENT_H
#define AUGMENT_H

#include "common.h"
#include <stddef.h>

#define MAX_LEVEL          5
#define AUGMENT_MAX_OWNED  (MAX_LEVEL - 1)   /* 레벨 1→5 = 4장 */

typedef enum {
    TIER_NONE = 0,
    TIER_BRONZE,
    TIER_SILVER,
    TIER_GOLD,
    TIER_PRISM
} Tier;

typedef enum {
    AUG_SCORE_BOOST = 0,
    AUG_TETRIS_BONUS,
    AUG_TSPIN_MASTER,
    AUG_COMBO_KING,
    AUG_AEGIS,
    AUG_IRON_GRIP,
    AUG_REFLEXES,
    AUG_BAG_VISION,
    AUG_SCHOLAR,
    AUG_LUCKY_STAR,
    AUG_COUNT
} AugmentId;

typedef struct {
    AugmentId id;
    Tier      tier;
} OwnedAugment;

typedef struct {
    const char* name;
    int         values[5];   /* 인덱스 = Tier (0 unused) */
} AugmentDef;

typedef struct {
    OwnedAugment list[AUGMENT_MAX_OWNED];
    int count;

    /* 재계산 derived */
    int scorePct;          /* 클리어 점수 % 가산 */
    int tetrisAtk;         /* 4 라인 클리어 시 +공격 */
    int tspinAtk;          /* T-spin 라인 클리어 시 +공격 */
    int comboAtk;          /* combo >= 3 단계당 +공격 */
    int shieldCharges;     /* 수신 garbage 차단 회수 */
    int lockResetBonus;    /* 추가 lock reset 횟수 */
    int lockDelayBonus;    /* 추가 lock delay (ms) */
    int nextVisionBonus;   /* NEXT 표시 추가 */
    int xpPct;             /* XP 가산 % */
    int luckyBonusScore;   /* 락당 추가 점수 */
} AugInventory;

typedef struct {
    OwnedAugment offers[3];
} CardOffer;

typedef struct {
    int level;        /* 1 .. MAX_LEVEL */
    int xp;           /* 현재 레벨 누적 */
    int xpToNext;     /* 다음 레벨 임계치 (level == MAX_LEVEL이면 의미 없음) */
    int pendingCards; /* 미선택 카드 모달 수 */
} LevelState;

const AugmentDef* augmentDef(AugmentId id);
const char*       tierName(Tier t);
char              tierShortChar(Tier t);
void              augDescribe(OwnedAugment a, char* out, size_t n);

void augInventoryInit(AugInventory* inv);
void augInventoryAdd(AugInventory* inv, OwnedAugment a);

void cardOfferGenerate(CardOffer* offer);

void levelInit(LevelState* L);
int  levelThreshold(int level);
/* XP 가산. 레벨업 발생 시 L->pendingCards 가 증가. MAX_LEVEL 도달 시 추가 XP는 무시. */
void levelGainXp(LevelState* L, int xpRaw, int xpPctBonus);

#endif
