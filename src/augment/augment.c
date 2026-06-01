#include "augment.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static const AugmentDef DEFS[AUG_COUNT] = {
    [AUG_SCORE_BOOST]  = { "Score Boost",   {0,  10,  25,   50,  100} },
    [AUG_TETRIS_BONUS] = { "Tetris Bonus",  {0,   1,   2,    3,    5} },
    [AUG_TSPIN_MASTER] = { "T-Spin Master", {0,   1,   2,    3,    5} },
    [AUG_COMBO_KING]   = { "Combo King",    {0,   1,   2,    3,    4} },
    [AUG_AEGIS]        = { "Aegis",         {0,   1,   3,    6,   12} },
    [AUG_IRON_GRIP]    = { "Iron Grip",     {0,   5,  10,   20,   40} },
    [AUG_REFLEXES]     = { "Reflexes",      {0, 200, 500, 1000, 2000} },
    [AUG_BAG_VISION]   = { "Bag Vision",    {0,   1,   1,    2,    2} },
    [AUG_SCHOLAR]      = { "Scholar",       {0,  15,  30,   50,  100} },
    [AUG_LUCKY_STAR]   = { "Lucky Star",    {0, 100, 300,  700, 1500} },
};

const AugmentDef* augmentDef(AugmentId id)
{
    if ((int)id < 0 || (int)id >= AUG_COUNT) return NULL;
    return &DEFS[id];
}

const char* tierName(Tier t)
{
    switch (t) {
        case TIER_BRONZE: return "BRONZE";
        case TIER_SILVER: return "SILVER";
        case TIER_GOLD:   return "GOLD";
        case TIER_PRISM:  return "PRISM";
        default:          return "?";
    }
}

char tierShortChar(Tier t)
{
    switch (t) {
        case TIER_BRONZE: return 'B';
        case TIER_SILVER: return 'S';
        case TIER_GOLD:   return 'G';
        case TIER_PRISM:  return 'P';
        default:          return '?';
    }
}

void augDescribe(OwnedAugment a, char* out, size_t n)
{
    const AugmentDef* d = augmentDef(a.id);
    if (!d) { snprintf(out, n, "?"); return; }
    int v = d->values[a.tier];
    switch (a.id) {
        case AUG_SCORE_BOOST:  snprintf(out, n, "Score +%d%%", v); break;
        case AUG_TETRIS_BONUS: snprintf(out, n, "Tetris +%d atk", v); break;
        case AUG_TSPIN_MASTER: snprintf(out, n, "T-Spin +%d atk", v); break;
        case AUG_COMBO_KING:   snprintf(out, n, "Combo +%d/step", v); break;
        case AUG_AEGIS:        snprintf(out, n, "+%d shield", v); break;
        case AUG_IRON_GRIP:    snprintf(out, n, "+%d lock resets", v); break;
        case AUG_REFLEXES:     snprintf(out, n, "+%d ms lock", v); break;
        case AUG_BAG_VISION:   snprintf(out, n, "+%d next", v); break;
        case AUG_SCHOLAR:      snprintf(out, n, "+%d%% XP", v); break;
        case AUG_LUCKY_STAR:   snprintf(out, n, "+%d / lock", v); break;
        default:               snprintf(out, n, "?"); break;
    }
}

void augInventoryInit(AugInventory* inv)
{
    memset(inv, 0, sizeof(*inv));
}

static void recompute(AugInventory* inv)
{
    inv->scorePct = 0;
    inv->tetrisAtk = 0;
    inv->tspinAtk = 0;
    inv->comboAtk = 0;
    inv->shieldCharges = 0;
    inv->lockResetBonus = 0;
    inv->lockDelayBonus = 0;
    inv->nextVisionBonus = 0;
    inv->xpPct = 0;
    inv->luckyBonusScore = 0;

    for (int i = 0; i < inv->count; i++) {
        OwnedAugment a = inv->list[i];
        const AugmentDef* d = augmentDef(a.id);
        if (!d) continue;
        int v = d->values[a.tier];
        switch (a.id) {
            case AUG_SCORE_BOOST:  inv->scorePct        += v; break;
            case AUG_TETRIS_BONUS: inv->tetrisAtk       += v; break;
            case AUG_TSPIN_MASTER: inv->tspinAtk        += v; break;
            case AUG_COMBO_KING:   inv->comboAtk        += v; break;
            case AUG_AEGIS:        inv->shieldCharges   += v; break;
            case AUG_IRON_GRIP:    inv->lockResetBonus  += v; break;
            case AUG_REFLEXES:     inv->lockDelayBonus  += v; break;
            case AUG_BAG_VISION:   inv->nextVisionBonus += v; break;
            case AUG_SCHOLAR:      inv->xpPct           += v; break;
            case AUG_LUCKY_STAR:   inv->luckyBonusScore += v; break;
            default: break;
        }
    }
}

void augInventoryAdd(AugInventory* inv, OwnedAugment a)
{
    if (inv->count >= AUGMENT_MAX_OWNED) return;
    inv->list[inv->count++] = a;
    recompute(inv);
}

static Tier rollTier(void)
{
    int r = rand() % 100;
    if (r < 60) return TIER_BRONZE;
    if (r < 85) return TIER_SILVER;
    if (r < 97) return TIER_GOLD;
    return TIER_PRISM;
}

void cardOfferGenerate(CardOffer* offer)
{
    bool used[AUG_COUNT] = {false};
    int picked = 0;
    int safety = 0;
    while (picked < 3 && safety < 200) {
        safety++;
        AugmentId id = (AugmentId)(rand() % AUG_COUNT);
        if (used[id]) continue;
        used[id] = true;
        offer->offers[picked].id   = id;
        offer->offers[picked].tier = rollTier();
        picked++;
    }
}

void levelInit(LevelState* L)
{
    L->level = 1;
    L->xp = 0;
    L->xpToNext = levelThreshold(1);
    L->pendingCards = 0;
}

int levelThreshold(int level)
{
    /* level=1 (다음=2)에 필요한 XP. 점진 증가. */
    return 10 + 5 * level;     /* 1→2: 15, 2→3: 20, 3→4: 25, 4→5: 30  (총 90) */
}

void levelGainXp(LevelState* L, int xpRaw, int xpPctBonus)
{
    if (L->level >= MAX_LEVEL) return;
    int gain = xpRaw * (100 + xpPctBonus) / 100;
    if (gain < 0) gain = 0;
    L->xp += gain;
    while (L->level < MAX_LEVEL && L->xp >= L->xpToNext) {
        L->xp -= L->xpToNext;
        L->level++;
        L->pendingCards++;
        if (L->level >= MAX_LEVEL) {
            L->xp = 0;
            L->xpToNext = 0;
            break;
        }
        L->xpToNext = levelThreshold(L->level);
    }
}
