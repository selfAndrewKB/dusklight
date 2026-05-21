#include "dusk/coop/damage_owner.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_arrow.h"
#include "d/actor/d_a_nbomb.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>

namespace dusk::coop::damage_owner {
namespace {

DamageOwnerDebugState s_debugState;
u64 s_nextEventId = 1;
u32 s_currentSimFrame = 0;

constexpr int kHitRingCount = sizeof(s_debugState.hits) / sizeof(s_debugState.hits[0]);

DamageActorDebug actorDebug(const fopAc_ac_c* actor) {
    DamageActorDebug debug;
    if (actor == nullptr) {
        return debug;
    }

    debug.ptr = reinterpret_cast<uintptr_t>(actor);
    debug.profile = static_cast<int>(fopAcM_GetProfName(actor));
    debug.name = static_cast<int>(fopAcM_GetName(const_cast<fopAc_ac_c*>(actor)));
    debug.id = static_cast<int>(fopAcM_GetID(actor));
    debug.room = static_cast<int>(fopAcM_GetRoomNo(actor));
    debug.argument = static_cast<int>(actor->argument);
    debug.pos[0] = actor->current.pos.x;
    debug.pos[1] = actor->current.pos.y;
    debug.pos[2] = actor->current.pos.z;
    debug.angleY = actor->shape_angle.y;
    debug.available = true;
    return debug;
}

bool isValidSlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && static_cast<unsigned char>(slot) < kPlayerSlotCount;
}

void assignOwner(DamageOwnerResult* result, fopAc_ac_c* owner, DamageOwnerReason reason) {
    if (result == nullptr || owner == nullptr) {
        return;
    }

    const PlayerSlot slot = getSlotForActor(owner);
    if (!isValidSlot(slot)) {
        return;
    }

    result->slot = slot;
    result->localPlayerActor = owner;
    result->localPlayer = static_cast<daPy_py_c*>(owner);
    result->ownerDebug = actorDebug(owner);
    result->reason = reason;
    result->found = true;
}

void assignFallbackPrimary(DamageOwnerResult* result, DamageOwnerReason reason) {
    fopAc_ac_c* primary = getPlayer(PlayerSlot::Primary);
    if (primary == nullptr) {
        primary = getPrimaryPlayer();
    }
    if (primary == nullptr) {
        return;
    }

    result->slot = PlayerSlot::Primary;
    result->localPlayerActor = primary;
    result->localPlayer = static_cast<daPy_py_c*>(primary);
    result->ownerDebug = actorDebug(primary);
    result->reason = reason;
    result->found = true;
}

fopAc_ac_c* findBoomerangOwner(const fopAc_ac_c* boomerang) {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        fopAc_ac_c* actor = getPlayer(static_cast<PlayerSlot>(i));
        daAlink_c* player = static_cast<daAlink_c*>(actor);
        if (player != nullptr && player->getBoomerangActor() == boomerang) {
            return actor;
        }
    }

    return nullptr;
}

void fillColliderFacts(DamageOwnerResult* result, cCcD_Obj* collider) {
    if (result == nullptr || collider == nullptr) {
        return;
    }

    result->attackType = collider->GetAtType();
    result->atp = collider->GetAtAtp();
    result->attackPower = collider->GetAtAtp();

    dCcD_GObjInf* gobj = static_cast<dCcD_GObjInf*>(collider->GetGObjInf());
    if (gobj != nullptr) {
        result->special = static_cast<int>(gobj->GetAtSpl());
    }
}

void fillOwnerCombatFacts(DamageOwnerResult* result) {
    if (result == nullptr || result->localPlayer == nullptr) {
        return;
    }

    result->cutType = static_cast<int>(result->localPlayer->getCutType());
    result->cutCount = static_cast<int>(result->localPlayer->getCutCount());
}

}  // namespace

void advanceDamageOwnerFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

DamageOwnerResult resolveDamageOwner(fopAc_ac_c* victim, cCcD_Obj* collider) {
    DamageOwnerResult result;
    result.victimDebug = actorDebug(victim);
    fillColliderFacts(&result, collider);

    fopAc_ac_c* hitActor = collider != nullptr ? collider->GetAc() : nullptr;
    result.hitActor = hitActor;
    result.hitActorDebug = actorDebug(hitActor);
    if (hitActor == nullptr) {
        assignFallbackPrimary(&result, DamageOwnerReason::FallbackPrimary);
        fillOwnerCombatFacts(&result);
        return result;
    }

    assignOwner(&result, hitActor, DamageOwnerReason::DirectPlayer);
    if (!result.found) {
        const s16 actorName = fopAcM_GetName(hitActor);
        if (actorName == fpcNm_ARROW_e) {
            assignOwner(&result, static_cast<daArrow_c*>(hitActor)->getOwner(),
                        DamageOwnerReason::OwnedProjectile);
        } else if (actorName == fpcNm_NBOMB_e) {
            assignOwner(&result, static_cast<daNbomb_c*>(hitActor)->getOwner(),
                        DamageOwnerReason::OwnedBomb);
        } else if (actorName == fpcNm_BOOMERANG_e) {
            assignOwner(&result, findBoomerangOwner(hitActor), DamageOwnerReason::OwnedBoomerang);
        }
    }

    if (!result.found) {
        assignFallbackPrimary(&result, DamageOwnerReason::UnknownActor);
    }

    fillOwnerCombatFacts(&result);
    return result;
}

daPy_py_c* resolveDamageOwnerPlayer(const DamageOwnerResult& result) {
    if (result.localPlayer != nullptr) {
        return result.localPlayer;
    }

    return static_cast<daPy_py_c*>(getPrimaryPlayer());
}

void recordDamageOwnerHit(const char* label, fopAc_ac_c* victim, const DamageOwnerResult& result,
                          const dCcU_AtInfo* atInfo, int reactionMode) {
    DamageOwnerHitDebug& hit = s_debugState.hits[(s_nextEventId - 1) % kHitRingCount];
    hit = {};
    hit.eventId = s_nextEventId++;
    hit.simFrame = s_currentSimFrame;
    std::snprintf(hit.label, sizeof(hit.label), "%s", label != nullptr ? label : "");
    hit.owner = result;
    hit.owner.victimDebug = actorDebug(victim);
    if (atInfo != nullptr) {
        hit.hitType = static_cast<int>(atInfo->mHitType);
        hit.attackPower = atInfo->mAttackPower;
        hit.hitStatus = static_cast<int>(atInfo->mHitStatus);
        if (victim != nullptr) {
            hit.hitPos = victim->current.pos;
            hit.hitPos.y += 80.0f;
        }
    }
    hit.reactionMode = reactionMode;

    if (s_debugState.hitCount < kHitRingCount) {
        s_debugState.hitCount++;
    }
}

const DamageOwnerDebugState& getDamageOwnerDebugState() {
    return s_debugState;
}

const char* damageOwnerReasonName(DamageOwnerReason reason) {
    switch (reason) {
    case DamageOwnerReason::DirectPlayer:
        return "DirectPlayer";
    case DamageOwnerReason::OwnedProjectile:
        return "OwnedProjectile";
    case DamageOwnerReason::OwnedBomb:
        return "OwnedBomb";
    case DamageOwnerReason::OwnedBoomerang:
        return "OwnedBoomerang";
    case DamageOwnerReason::FallbackPrimary:
        return "FallbackPrimary";
    case DamageOwnerReason::UnknownActor:
    default:
        return "UnknownActor";
    }
}

const char* damageOwnerHitTypeName(int hitType) {
    switch (hitType) {
    case HIT_TYPE_LINK_NORMAL_ATTACK:
        return "link_normal";
    case HIT_TYPE_LINK_HEAVY_ATTACK:
        return "link_heavy";
    case HIT_TYPE_BOMB:
        return "bomb";
    case HIT_TYPE_BOOMERANG:
        return "boomerang";
    case HIT_TYPE_ARROW:
        return "arrow";
    case HIT_TYPE_STUN:
        return "stun";
    default:
        return "unknown";
    }
}

const char* damageOwnerAttackName(u32 attackType) {
    if ((attackType & AT_TYPE_IRON_BALL) != 0) {
        return "iron_ball";
    }
    if ((attackType & AT_TYPE_BOMB) != 0) {
        return "bomb";
    }
    if ((attackType & AT_TYPE_ARROW) != 0) {
        return "arrow";
    }
    if ((attackType & AT_TYPE_SLINGSHOT) != 0) {
        return "slingshot";
    }
    if ((attackType & AT_TYPE_BOOMERANG) != 0) {
        return "boomerang";
    }
    if ((attackType & AT_TYPE_HOOKSHOT) != 0) {
        return "hookshot";
    }
    if ((attackType & AT_TYPE_SHIELD_ATTACK) != 0) {
        return "shield";
    }
    if ((attackType & (AT_TYPE_MASTER_SWORD | AT_TYPE_NORMAL_SWORD)) != 0) {
        return "sword";
    }
    return "unknown";
}

bool damageOwnerAttackUsesCutState(u32 attackType) {
    return (attackType & (AT_TYPE_NORMAL_SWORD | AT_TYPE_MASTER_SWORD)) != 0;
}

const char* damageOwnerCutTypeName(int cutType) {
    switch (cutType) {
    case daPy_py_c::CUT_TYPE_NONE:
        return "NONE";
    case daPy_py_c::CUT_TYPE_NM_VERTICAL:
        return "NM_VERTICAL";
    case daPy_py_c::CUT_TYPE_NM_STAB:
        return "NM_STAB";
    case daPy_py_c::CUT_TYPE_NM_RIGHT:
        return "NM_RIGHT";
    case daPy_py_c::CUT_TYPE_NM_LEFT:
        return "NM_LEFT";
    case daPy_py_c::CUT_TYPE_HEAD_JUMP:
        return "HEAD_JUMP";
    case daPy_py_c::CUT_TYPE_DASH_RIGHT:
        return "DASH_RIGHT";
    case daPy_py_c::CUT_TYPE_TURN_RIGHT:
        return "TURN_RIGHT";
    case daPy_py_c::CUT_TYPE_TURN_LEFT:
        return "TURN_LEFT";
    default:
        return "OTHER";
    }
}

}  // namespace dusk::coop::damage_owner
