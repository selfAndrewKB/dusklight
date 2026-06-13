#pragma once

#include "dolphin/types.h"
#include "dusk/coop/damage_owner.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/player_slots.h"

class daPy_py_c;
class fopAc_ac_c;

namespace dusk::coop::wolf_catch_owner {

enum class WolfCatchOwnerReason : u8 {
    DirectPlayer,
    DamageOwner,
    FallbackPrimary,
    Cleared,
};

struct WolfCatchOwnerState {
    fopAc_ac_c* enemy = nullptr;
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* localPlayer = nullptr;
    fopAc_ac_c* localPlayerActor = nullptr;
    PlayerQueryActorDebug enemyDebug;
    PlayerQueryActorDebug playerDebug;
    WolfCatchOwnerReason reason = WolfCatchOwnerReason::Cleared;
    bool active = false;
    bool found = false;
};

struct WolfCatchOwnerDecisionDebug {
    u64 eventId = 0;
    u32 simFrame = 0;
    char label[64] = {};
    WolfCatchOwnerState state;
};

struct WolfCatchOwnerDebugState {
    WolfCatchOwnerDecisionDebug decisions[32] = {};
    int decisionCount = 0;
    u32 currentSimFrame = 0;
};

void advanceWolfCatchOwnerFrame(u32 frame);

WolfCatchOwnerState beginWolfCatch(const char* label, fopAc_ac_c* enemy, daPy_py_c* player);
WolfCatchOwnerState beginWolfCatchFromDamageOwner(
    const char* label, fopAc_ac_c* enemy,
    const damage_owner::DamageOwnerResult& damageOwner);
WolfCatchOwnerState updateWolfCatch(const char* label, fopAc_ac_c* enemy);
WolfCatchOwnerState getWolfCatch(fopAc_ac_c* enemy);
void clearWolfCatch(const char* label, fopAc_ac_c* enemy);

const WolfCatchOwnerDebugState& getWolfCatchOwnerDebugState();
const char* wolfCatchOwnerReasonName(WolfCatchOwnerReason reason);

}  // namespace dusk::coop::wolf_catch_owner
