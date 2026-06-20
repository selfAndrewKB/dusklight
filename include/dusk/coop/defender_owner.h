#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class dCcD_GObjInf;
class daPy_py_c;
class fopAc_ac_c;

namespace dusk::coop::defender_owner {

enum class DefenderOwnerReason : u8 {
    DirectPlayer,
    UnknownActor,
    FallbackPrimary,
    NoHit,
};

struct DefenderActorDebug {
    uintptr_t ptr = 0;
    int profile = 0;
    int name = 0;
    int id = 0;
    int room = 0;
    int argument = 0;
    f32 pos[3] = {};
    s16 angleY = 0;
    bool available = false;
};

struct DefenderOwnerResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* localPlayer = nullptr;
    fopAc_ac_c* localPlayerActor = nullptr;
    fopAc_ac_c* hitActor = nullptr;
    DefenderActorDebug attackerDebug;
    DefenderActorDebug hitActorDebug;
    DefenderActorDebug defenderDebug;
    cXyz hitPos = cXyz::Zero;
    bool guarded = false;
    bool guardBreak = false;
    bool atShieldHit = false;
    bool targetShield = false;
    bool targetSpecialShield = false;
    bool targetSmallShield = false;
    bool targetShieldHit = false;
    DefenderOwnerReason reason = DefenderOwnerReason::NoHit;
    bool found = false;
};

struct DefenderOwnerDecisionDebug {
    u64 eventId = 0;
    u32 simFrame = 0;
    char label[64] = {};
    DefenderOwnerResult defender;
};

struct DefenderOwnerDebugState {
    DefenderOwnerDecisionDebug decisions[32] = {};
    int decisionCount = 0;
    u32 currentSimFrame = 0;
};

void advanceDefenderOwnerFrame(u32 frame);
DefenderOwnerResult resolveDefenderOwner(fopAc_ac_c* attacker, dCcD_GObjInf* attackCollider);
DefenderOwnerResult resolveDefenderOwnerFromActor(fopAc_ac_c* attacker,
                                                  fopAc_ac_c* hitActor);
void recordDefenderOwnerContact(const char* label, fopAc_ac_c* attacker,
                                const DefenderOwnerResult& result);

const DefenderOwnerDebugState& getDefenderOwnerDebugState();
const char* defenderOwnerReasonName(DefenderOwnerReason reason);

}  // namespace dusk::coop::defender_owner
