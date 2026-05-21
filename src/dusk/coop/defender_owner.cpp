#include "dusk/coop/defender_owner.h"

#include "d/actor/d_a_player.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>

namespace dusk::coop::defender_owner {
namespace {

DefenderOwnerDebugState s_debugState;
u64 s_nextEventId = 1;
u32 s_currentSimFrame = 0;
int s_nextDebugEvict = 0;

constexpr int kDecisionCount = sizeof(s_debugState.decisions) / sizeof(s_debugState.decisions[0]);
constexpr int kLabelSize = 64;

DefenderActorDebug actorDebug(const fopAc_ac_c* actor) {
    DefenderActorDebug debug;
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

void copyLabel(char* dst, const char* label) {
    std::snprintf(dst, kLabelSize, "%s", label != nullptr ? label : "");
}

void fillDefenderFacts(DefenderOwnerResult* result) {
    if (result == nullptr || result->localPlayer == nullptr) {
        return;
    }

    result->guarded = result->localPlayer->checkPlayerGuard();
    result->guardBreak = result->localPlayer->checkGuardBreakMode();
}

void fillColliderFacts(DefenderOwnerResult* result, dCcD_GObjInf* attackCollider) {
    if (result == nullptr || attackCollider == nullptr) {
        return;
    }

    result->atShieldHit = attackCollider->ChkAtShieldHit();
    dCcD_GObjInf* target = static_cast<dCcD_GObjInf*>(attackCollider->GetAtHitGObj());
    if (target != nullptr) {
        result->targetShield = target->ChkTgShield();
        result->targetSpecialShield = target->ChkTgSpShield();
        result->targetSmallShield = target->ChkTgSmallShield();
        result->targetShieldHit = target->ChkTgShieldHit();
    }

    cXyz* hitPos = attackCollider->GetAtHitPosP();
    if (hitPos != nullptr) {
        result->hitPos = *hitPos;
    }
}

bool sameDecision(const DefenderOwnerDecisionDebug& decision, const char* label,
                  const DefenderOwnerResult& result) {
    return std::strcmp(decision.label, label != nullptr ? label : "") == 0 &&
           decision.defender.attackerDebug.ptr == result.attackerDebug.ptr &&
           decision.defender.hitActorDebug.ptr == result.hitActorDebug.ptr &&
           decision.defender.slot == result.slot &&
           decision.defender.found == result.found &&
           decision.defender.guarded == result.guarded &&
           decision.defender.guardBreak == result.guardBreak &&
           decision.defender.atShieldHit == result.atShieldHit &&
           decision.defender.targetShield == result.targetShield &&
           decision.defender.targetSpecialShield == result.targetSpecialShield &&
           decision.defender.targetSmallShield == result.targetSmallShield &&
           decision.defender.targetShieldHit == result.targetShieldHit &&
           decision.defender.reason == result.reason;
}

DefenderOwnerDecisionDebug* findDecision(const char* label, const DefenderOwnerResult& result) {
    const char* labelName = label != nullptr ? label : "";
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        DefenderOwnerDecisionDebug& decision = s_debugState.decisions[i];
        if (std::strcmp(decision.label, labelName) == 0 &&
            decision.defender.attackerDebug.ptr == result.attackerDebug.ptr)
        {
            return &decision;
        }
    }

    if (s_debugState.decisionCount < kDecisionCount) {
        return &s_debugState.decisions[s_debugState.decisionCount++];
    }
    return &s_debugState.decisions[s_nextDebugEvict++ % kDecisionCount];
}

}  // namespace

void advanceDefenderOwnerFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

DefenderOwnerResult resolveDefenderOwner(fopAc_ac_c* attacker, dCcD_GObjInf* attackCollider) {
    DefenderOwnerResult result;
    result.attackerDebug = actorDebug(attacker);

    if (attackCollider == nullptr || !attackCollider->ChkAtHit()) {
        result.reason = DefenderOwnerReason::NoHit;
        return result;
    }

    fillColliderFacts(&result, attackCollider);
    cCcD_Obj* hitObj = attackCollider->GetAtHitObj();
    fopAc_ac_c* hitActor = hitObj != nullptr ? dCc_GetAc(hitObj->GetAc()) : nullptr;
    result.hitActor = hitActor;
    result.hitActorDebug = actorDebug(hitActor);
    if (hitActor == nullptr) {
        result.reason = DefenderOwnerReason::NoHit;
        return result;
    }

    const PlayerSlot slot = getSlotForActor(hitActor);
    if (isValidSlot(slot)) {
        result.slot = slot;
        result.localPlayerActor = hitActor;
        result.localPlayer = static_cast<daPy_py_c*>(hitActor);
        result.defenderDebug = actorDebug(hitActor);
        result.reason = DefenderOwnerReason::DirectPlayer;
        result.found = true;
        fillDefenderFacts(&result);
        return result;
    }

    result.reason = DefenderOwnerReason::UnknownActor;
    return result;
}

void recordDefenderOwnerContact(const char* label, fopAc_ac_c* attacker,
                                const DefenderOwnerResult& result) {
    DefenderOwnerResult debugResult = result;
    debugResult.attackerDebug = actorDebug(attacker);
    DefenderOwnerDecisionDebug* decision = findDecision(label, debugResult);
    if (decision == nullptr) {
        return;
    }

    // Co-op: held attack/defender overlap can be reported on consecutive frames. Treat a short
    // gap as the same contact, but emit again when a later swing touches the same defender.
    const bool contactResumed =
        decision->eventId == 0 || s_currentSimFrame > decision->simFrame + 2;
    const bool changed = contactResumed || !sameDecision(*decision, label, debugResult);
    copyLabel(decision->label, label);
    decision->simFrame = s_currentSimFrame;
    decision->defender = debugResult;
    if (changed) {
        decision->eventId = s_nextEventId++;
    }
}

const DefenderOwnerDebugState& getDefenderOwnerDebugState() {
    return s_debugState;
}

const char* defenderOwnerReasonName(DefenderOwnerReason reason) {
    switch (reason) {
    case DefenderOwnerReason::DirectPlayer:
        return "DirectPlayer";
    case DefenderOwnerReason::FallbackPrimary:
        return "FallbackPrimary";
    case DefenderOwnerReason::NoHit:
        return "NoHit";
    case DefenderOwnerReason::UnknownActor:
    default:
        return "UnknownActor";
    }
}

}  // namespace dusk::coop::defender_owner
