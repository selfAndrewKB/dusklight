#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class cCcD_Obj;
class daPy_py_c;
class fopAc_ac_c;
struct dCcU_AtInfo;

namespace dusk::coop::damage_owner {

enum class DamageOwnerReason : u8 {
    DirectPlayer,
    OwnedProjectile,
    OwnedBomb,
    OwnedBoomerang,
    UnknownActor,
    FallbackPrimary,
};

struct DamageActorDebug {
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

struct DamageOwnerResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* localPlayer = nullptr;
    fopAc_ac_c* localPlayerActor = nullptr;
    fopAc_ac_c* hitActor = nullptr;
    DamageActorDebug victimDebug;
    DamageActorDebug hitActorDebug;
    DamageActorDebug ownerDebug;
    u32 attackType = 0;
    u8 atp = 0;
    int special = 0;
    int cutType = -1;
    int cutCount = -1;
    DamageOwnerReason reason = DamageOwnerReason::UnknownActor;
    bool found = false;
};

struct DamageOwnerHitDebug {
    u64 eventId = 0;
    u32 simFrame = 0;
    char label[64] = {};
    DamageOwnerResult owner;
    int hitType = 0;
    u16 attackPower = 0;
    int hitStatus = 0;
    int reactionMode = -1;
    cXyz hitPos = {};
};

struct DamageOwnerDebugState {
    DamageOwnerHitDebug hits[32] = {};
    int hitCount = 0;
    u32 currentSimFrame = 0;
};

void advanceDamageOwnerFrame(u32 frame);

DamageOwnerResult resolveDamageOwner(fopAc_ac_c* victim, cCcD_Obj* collider);
daPy_py_c* resolveDamageOwnerPlayer(const DamageOwnerResult& result);
void recordDamageOwnerHit(const char* label, fopAc_ac_c* victim, const DamageOwnerResult& result,
                          const dCcU_AtInfo* atInfo, int reactionMode = -1);

const DamageOwnerDebugState& getDamageOwnerDebugState();
const char* damageOwnerReasonName(DamageOwnerReason reason);
const char* damageOwnerHitTypeName(int hitType);
const char* damageOwnerAttackName(u32 attackType);
bool damageOwnerAttackUsesCutState(u32 attackType);
const char* damageOwnerCutTypeName(int cutType);

}  // namespace dusk::coop::damage_owner
