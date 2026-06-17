#include "dusk/coop/selected_target_state.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "dusk/coop/player_camera_status.h"
#include "f_pc/f_pc_name.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>

namespace dusk::coop::selected_target_state {
namespace {

SelectedTargetDebugState s_debugState;
int s_nextDebugEvict = 0;

constexpr int kDecisionCount = sizeof(s_debugState.decisions) / sizeof(s_debugState.decisions[0]);
constexpr int kLabelSize = 64;

void copyLabel(char* dst, const char* label) {
    std::snprintf(dst, kLabelSize, "%s", label != nullptr ? label : "");
}

PlayerQueryActorDebug actorDebug(const fopAc_ac_c* actor) {
    PlayerQueryActorDebug debug;
    if (actor == nullptr) {
        return debug;
    }

    debug.ptr = reinterpret_cast<uintptr_t>(actor);
    debug.profile = static_cast<int>(fopAcM_GetProfName(actor));
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

void recordDecision(const fopAc_ac_c* observer, const char* label, const SelectedTargetState& state,
                    SelectedTargetStateReason reason) {
    const char* labelName = label != nullptr ? label : "";
    SelectedTargetDecisionDebug* decision = nullptr;
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        if (s_debugState.decisions[i].observer == observer &&
            std::strcmp(s_debugState.decisions[i].label, labelName) == 0)
        {
            decision = &s_debugState.decisions[i];
            break;
        }
    }

    if (decision == nullptr) {
        if (s_debugState.decisionCount < kDecisionCount) {
            decision = &s_debugState.decisions[s_debugState.decisionCount++];
        } else {
            decision = &s_debugState.decisions[s_nextDebugEvict++ % kDecisionCount];
        }
    }

    copyLabel(decision->label, labelName);
    decision->observer = const_cast<fopAc_ac_c*>(observer);
    decision->observerDebug = actorDebug(observer);
    decision->state = state;
    decision->reason = reason;
    decision->found = state.available;
}

}  // namespace

SelectedTargetState stateForSlot(PlayerSlot slot, fopAc_ac_c* actor) {
    SelectedTargetState state;
    if (!isValidSlot(slot) || actor == nullptr || getPlayer(slot) != actor) {
        return state;
    }

    daPy_py_c* player = static_cast<daPy_py_c*>(actor);
    state.slot = slot;
    state.actor = actor;
    state.player = player;
    state.actorDebug = actorDebug(actor);
    state.pos = actor->current.pos;
    state.shapeAngleY = actor->shape_angle.y;
    state.speedF = player->getSpeedF();
    state.damageWaitTimer = player->getDamageWaitTimer();
    state.cutType = static_cast<int>(player->getCutType());
    state.cutCount = static_cast<int>(player->getCutCount());
    state.cutActive = player->getCutType() != daPy_py_c::CUT_TYPE_NONE;
    state.wolf = player->checkWolf() != 0;
    if (fopAcM_GetProfName(actor) == fpcNm_ALINK_e) {
        daAlink_c* alink = static_cast<daAlink_c*>(actor);
        state.wolfEyeUp = alink->checkWolfEyeUp();
    }
    state.wolfBark = player->checkWolfBark() != 0;
    state.wolfThreat = player->checkWolfThreat() != 0;
    // Co-op: wolf-sense visibility is owner-local player state. Ghost-like actors should ask the
    // active slot that owns sense state instead of P1's global checkNowWolfPowerUp() helper.
    state.wolfSenseActive = state.wolf && state.wolfEyeUp != 0;
    state.horseRide = player->checkHorseRide();
    state.boarRide = player->checkBoarRide();
    state.damageWaiting = state.damageWaitTimer != 0;
    // Co-op: these are slot-local camera/status gates that several enemies use after they
    // already know which player they mean. Snapshot them here so actor files do not fall back
    // to dComIfGp_checkPlayerStatus0(0, flag) for P2+ selected targets.
    state.status0_0x100 = player_camera_status::checkStatus0(slot, 0x100) != 0;
    state.status0_0x4000 = player_camera_status::checkStatus0(slot, 0x4000) != 0;
    state.ironBallSubject = player_camera_status::checkStatus0(slot, 0x400) != 0;
    // Co-op: equipment flags live on the selected player actor, not in the camera/status table.
    // Water enemies such as Bombfish use this exact vanilla helper to decide whether Link is
    // heavy enough underwater to be attacked, so P2+ must answer from the owner-local actor.
    state.equipHeavyBoots = player->checkEquipHeavyBoots() != 0;
    state.available = true;
    return state;
}

SelectedTargetState stateForEnemyTarget(const EnemyTargetResult& target) {
    return stateForSlot(target.slot, target.localActor);
}

SelectedTargetState findNearestPlayerState(const fopAc_ac_c* observer, const char* label,
                                           PlayerStatePredicate predicate, f32 maxDistanceXZ) {
    SelectedTargetState result;
    SelectedTargetStateReason reason = SelectedTargetStateReason::NoMatch;
    f32 bestDistanceXZ = 0.0f;

    forEachActivePlayer([&](PlayerSlot slot, fopAc_ac_c* actor) {
        SelectedTargetState candidate = stateForSlot(slot, actor);
        if (!candidate.available || (predicate != nullptr && !predicate(candidate))) {
            return;
        }

        const f32 distanceXZ = observer != nullptr
                                   ? fopAcM_searchActorDistanceXZ(observer, actor)
                                   : 0.0f;
        if (maxDistanceXZ >= 0.0f && distanceXZ >= maxDistanceXZ) {
            return;
        }

        if (!result.available || distanceXZ < bestDistanceXZ) {
            result = candidate;
            bestDistanceXZ = distanceXZ;
            reason = SelectedTargetStateReason::FilteredNearest;
        }
    });

    recordDecision(observer, label, result, reason);
    return result;
}

void recordSelectedTargetState(const fopAc_ac_c* observer, const char* label,
                               const SelectedTargetState& state,
                               SelectedTargetStateReason reason) {
    recordDecision(observer, label, state, reason);
}

const SelectedTargetDebugState& getSelectedTargetDebugState() {
    return s_debugState;
}

const char* selectedTargetStateReasonName(SelectedTargetStateReason reason) {
    switch (reason) {
    case SelectedTargetStateReason::EnemyTarget:
        return "EnemyTarget";
    case SelectedTargetStateReason::FilteredNearest:
        return "FilteredNearest";
    case SelectedTargetStateReason::InvalidTarget:
        return "InvalidTarget";
    case SelectedTargetStateReason::NoMatch:
    default:
        return "NoMatch";
    }
}

}  // namespace dusk::coop::selected_target_state
