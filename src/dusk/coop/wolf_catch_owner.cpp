#include "dusk/coop/wolf_catch_owner.h"

#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace dusk::coop::wolf_catch_owner {
namespace {

struct RetainedWolfCatchState {
    fopAc_ac_c* enemy = nullptr;
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* player = nullptr;
    fopAc_ac_c* playerActor = nullptr;
    WolfCatchOwnerReason reason = WolfCatchOwnerReason::Cleared;
    bool active = false;
};

std::vector<RetainedWolfCatchState> s_states;
WolfCatchOwnerDebugState s_debugState;
u64 s_nextEventId = 1;
u32 s_currentSimFrame = 0;
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

RetainedWolfCatchState* findState(fopAc_ac_c* enemy, bool create) {
    if (enemy == nullptr) {
        return nullptr;
    }

    for (RetainedWolfCatchState& state : s_states) {
        if (state.enemy == enemy) {
            return &state;
        }
    }

    if (!create) {
        return nullptr;
    }

    s_states.push_back({});
    s_states.back().enemy = enemy;
    return &s_states.back();
}

WolfCatchOwnerDecisionDebug* findDecision(const char* label, fopAc_ac_c* enemy) {
    const char* labelName = label != nullptr ? label : "";
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        WolfCatchOwnerDecisionDebug& decision = s_debugState.decisions[i];
        if (std::strcmp(decision.label, labelName) == 0 && decision.state.enemy == enemy) {
            return &decision;
        }
    }

    if (s_debugState.decisionCount < kDecisionCount) {
        return &s_debugState.decisions[s_debugState.decisionCount++];
    }
    return &s_debugState.decisions[s_nextDebugEvict++ % kDecisionCount];
}

WolfCatchOwnerState publicState(const RetainedWolfCatchState* state) {
    WolfCatchOwnerState result;
    if (state == nullptr) {
        return result;
    }

    result.enemy = state->enemy;
    result.slot = state->slot;
    result.localPlayer = state->player;
    result.localPlayerActor = state->playerActor;
    result.enemyDebug = actorDebug(state->enemy);
    result.playerDebug = actorDebug(state->playerActor);
    result.reason = state->reason;
    result.active = state->active;
    result.found = state->active && isValidSlot(state->slot) && state->player != nullptr;
    return result;
}

void recordDecision(const char* label, const WolfCatchOwnerState& state, bool semanticChange) {
    WolfCatchOwnerDecisionDebug* decision = findDecision(label, state.enemy);
    if (decision == nullptr) {
        return;
    }

    copyLabel(decision->label, label);
    decision->simFrame = s_currentSimFrame;
    decision->state = state;
    if (semanticChange) {
        decision->eventId = s_nextEventId++;
    }
}

void clearState(RetainedWolfCatchState* state) {
    if (state == nullptr) {
        return;
    }

    state->slot = PlayerSlot::Invalid;
    state->player = nullptr;
    state->playerActor = nullptr;
    state->reason = WolfCatchOwnerReason::Cleared;
    state->active = false;
}

}  // namespace

void advanceWolfCatchOwnerFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

WolfCatchOwnerState beginWolfCatch(const char* label, fopAc_ac_c* enemy, daPy_py_c* player) {
    RetainedWolfCatchState* state = findState(enemy, true);
    if (state == nullptr) {
        return WolfCatchOwnerState{};
    }

    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(player);
    PlayerSlot slot = getSlotForActor(actor);
    WolfCatchOwnerReason reason = WolfCatchOwnerReason::DirectPlayer;
    if (!isValidSlot(slot)) {
        slot = PlayerSlot::Primary;
        actor = getPrimaryPlayer();
        player = static_cast<daPy_py_c*>(actor);
        reason = WolfCatchOwnerReason::FallbackPrimary;
    }

    const bool changed =
        !state->active || state->slot != slot || state->playerActor != actor || state->reason != reason;
    state->enemy = enemy;
    state->slot = slot;
    state->player = player;
    state->playerActor = actor;
    state->reason = reason;
    state->active = player != nullptr;

    const WolfCatchOwnerState result = publicState(state);
    recordDecision(label, result, changed);
    return result;
}

WolfCatchOwnerState beginWolfCatchFromDamageOwner(
    const char* label, fopAc_ac_c* enemy,
    const damage_owner::DamageOwnerResult& damageOwner) {
    WolfCatchOwnerState state = beginWolfCatch(
        label, enemy, damage_owner::resolveDamageOwnerPlayer(damageOwner));
    RetainedWolfCatchState* retained = findState(enemy, false);
    if (retained != nullptr && retained->active && damageOwner.found) {
        retained->reason = damageOwner.reason == damage_owner::DamageOwnerReason::DirectPlayer
                               ? WolfCatchOwnerReason::DirectPlayer
                               : WolfCatchOwnerReason::DamageOwner;
        state = publicState(retained);
        recordDecision(label, state, false);
    }
    return state;
}

WolfCatchOwnerState updateWolfCatch(const char* label, fopAc_ac_c* enemy) {
    RetainedWolfCatchState* state = findState(enemy, false);
    if (state == nullptr) {
        WolfCatchOwnerState missing;
        missing.enemy = enemy;
        missing.enemyDebug = actorDebug(enemy);
        recordDecision(label, missing, false);
        return missing;
    }

    bool changed = false;
    if (state->active && (!isValidSlot(state->slot) || getPlayer(state->slot) != state->playerActor)) {
        clearState(state);
        changed = true;
    }

    const WolfCatchOwnerState result = publicState(state);
    recordDecision(label, result, changed);
    return result;
}

WolfCatchOwnerState getWolfCatch(fopAc_ac_c* enemy) {
    return publicState(findState(enemy, false));
}

void clearWolfCatch(const char* label, fopAc_ac_c* enemy) {
    RetainedWolfCatchState* state = findState(enemy, false);
    if (state == nullptr) {
        return;
    }

    const bool changed = state->active;
    clearState(state);
    const WolfCatchOwnerState result = publicState(state);
    recordDecision(label, result, changed);
}

const WolfCatchOwnerDebugState& getWolfCatchOwnerDebugState() {
    return s_debugState;
}

const char* wolfCatchOwnerReasonName(WolfCatchOwnerReason reason) {
    switch (reason) {
    case WolfCatchOwnerReason::DirectPlayer:
        return "DirectPlayer";
    case WolfCatchOwnerReason::DamageOwner:
        return "DamageOwner";
    case WolfCatchOwnerReason::FallbackPrimary:
        return "FallbackPrimary";
    case WolfCatchOwnerReason::Cleared:
    default:
        return "Cleared";
    }
}

}  // namespace dusk::coop::wolf_catch_owner
