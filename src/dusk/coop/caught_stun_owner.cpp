#include "dusk/coop/caught_stun_owner.h"

#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace dusk::coop::caught_stun_owner {
namespace {

struct RetainedStunState {
    fopAc_ac_c* enemy = nullptr;
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* player = nullptr;
    fopAc_ac_c* playerActor = nullptr;
    PlayerSlot affectedSlots[kPlayerSlotCount] = {};
    fopAc_ac_c* affectedLocalActors[kPlayerSlotCount] = {};
    CaughtStunOwnerReason reason = CaughtStunOwnerReason::Cleared;
    int affectedCount = 0;
    bool active = false;
};

std::vector<RetainedStunState> s_states;
CaughtStunOwnerDebugState s_debugState;
u64 s_nextEventId = 1;
u32 s_currentSimFrame = 0;
int s_nextDebugEvict = 0;

constexpr int kDecisionCount = sizeof(s_debugState.decisions) / sizeof(s_debugState.decisions[0]);
constexpr int kLabelSize = 64;

CaughtStunActorDebug actorDebug(const fopAc_ac_c* actor) {
    CaughtStunActorDebug debug;
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

f32 distanceXZ(const cXyz& a, const cXyz& b) {
    const f32 dx = a.x - b.x;
    const f32 dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

void copyLabel(char* dst, const char* label) {
    std::snprintf(dst, kLabelSize, "%s", label != nullptr ? label : "");
}

RetainedStunState* findState(fopAc_ac_c* enemy, bool create) {
    if (enemy == nullptr) {
        return nullptr;
    }

    for (RetainedStunState& state : s_states) {
        if (state.enemy == enemy) {
            return &state;
        }
    }

    if (!create) {
        return nullptr;
    }

    s_states.push_back({});
    s_states.back().enemy = enemy;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        s_states.back().affectedSlots[i] = PlayerSlot::Invalid;
    }
    return &s_states.back();
}

CaughtStunOwnerDecisionDebug* findDecision(const char* label, fopAc_ac_c* enemy) {
    const char* labelName = label != nullptr ? label : "";
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        CaughtStunOwnerDecisionDebug& decision = s_debugState.decisions[i];
        if (std::strcmp(decision.label, labelName) == 0 && decision.state.enemy == enemy) {
            return &decision;
        }
    }

    if (s_debugState.decisionCount < kDecisionCount) {
        return &s_debugState.decisions[s_debugState.decisionCount++];
    }
    return &s_debugState.decisions[s_nextDebugEvict++ % kDecisionCount];
}

CaughtStunOwnerState publicState(const RetainedStunState* state, int stunTimer, int cryTimer) {
    CaughtStunOwnerState result;
    if (state == nullptr) {
        return result;
    }

    result.enemy = state->enemy;
    result.slot = state->slot;
    result.localPlayer = state->player;
    result.localPlayerActor = state->playerActor;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        result.affectedSlots[i] = state->affectedSlots[i];
        result.affectedLocalActors[i] = state->affectedLocalActors[i];
    }
    result.enemyDebug = actorDebug(state->enemy);
    result.playerDebug = actorDebug(state->playerActor);
    result.reason = state->reason;
    result.affectedCount = state->affectedCount;
    result.stunTimer = stunTimer;
    result.cryTimer = cryTimer;
    result.active = state->active;
    result.found = state->active && isValidSlot(state->slot) && state->player != nullptr;
    return result;
}

void recordDecision(const char* label, const CaughtStunOwnerState& state, bool semanticChange) {
    CaughtStunOwnerDecisionDebug* decision = findDecision(label, state.enemy);
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

bool addAffectedPlayer(RetainedStunState* state, PlayerSlot slot, fopAc_ac_c* actor) {
    if (state == nullptr || !isValidSlot(slot) || actor == nullptr) {
        return false;
    }

    for (int i = 0; i < state->affectedCount; i++) {
        if (state->affectedSlots[i] == slot) {
            state->affectedLocalActors[i] = actor;
            return false;
        }
    }

    if (state->affectedCount >= kPlayerSlotCount) {
        return false;
    }

    state->affectedSlots[state->affectedCount] = slot;
    state->affectedLocalActors[state->affectedCount] = actor;
    state->affectedCount++;
    return true;
}

void clearAffectedPlayers(RetainedStunState* state) {
    if (state == nullptr) {
        return;
    }

    state->affectedCount = 0;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        state->affectedSlots[i] = PlayerSlot::Invalid;
        state->affectedLocalActors[i] = nullptr;
    }
}

}  // namespace

void advanceCaughtStunOwnerFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

CaughtStunOwnerState beginCaughtStun(const char* label, fopAc_ac_c* enemy, daPy_py_c* player) {
    return beginCaughtStunArea(label, enemy, player, -1.0f);
}

CaughtStunOwnerState beginCaughtStunArea(const char* label, fopAc_ac_c* enemy,
                                         daPy_py_c* player, f32 rangeXZ) {
    RetainedStunState* state = findState(enemy, true);
    if (state == nullptr) {
        return CaughtStunOwnerState{};
    }

    PlayerSlot slot = getSlotForActor(static_cast<fopAc_ac_c*>(player));
    CaughtStunOwnerReason reason = CaughtStunOwnerReason::DirectPlayer;
    if (!isValidSlot(slot)) {
        slot = PlayerSlot::Primary;
        player = static_cast<daPy_py_c*>(getPrimaryPlayer());
        reason = CaughtStunOwnerReason::FallbackPrimary;
    }

    PlayerSlot previousSlots[kPlayerSlotCount] = {};
    fopAc_ac_c* previousActors[kPlayerSlotCount] = {};
    const bool previousActive = state->active;
    const PlayerSlot previousOwnerSlot = state->slot;
    daPy_py_c* previousPlayer = state->player;
    const int previousCount = state->affectedCount;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        previousSlots[i] = state->affectedSlots[i];
        previousActors[i] = state->affectedLocalActors[i];
    }

    state->enemy = enemy;
    state->slot = slot;
    state->player = player;
    state->playerActor = static_cast<fopAc_ac_c*>(player);
    state->reason = reason;
    state->active = player != nullptr;
    clearAffectedPlayers(state);
    addAffectedPlayer(state, slot, state->playerActor);

    if (enemy != nullptr && rangeXZ >= 0.0f) {
        for (int i = 0; i < kPlayerSlotCount; i++) {
            PlayerSlot candidateSlot = static_cast<PlayerSlot>(i);
            fopAc_ac_c* candidate = getPlayer(candidateSlot);
            if (candidate != nullptr &&
                distanceXZ(candidate->current.pos, enemy->current.pos) <= rangeXZ)
            {
                addAffectedPlayer(state, candidateSlot, candidate);
            }
        }
    }

    bool affectedChanged = previousCount != state->affectedCount;
    if (!affectedChanged) {
        for (int i = 0; i < state->affectedCount; i++) {
            if (previousSlots[i] != state->affectedSlots[i] ||
                previousActors[i] != state->affectedLocalActors[i])
            {
                affectedChanged = true;
                break;
            }
        }
    }

    const bool changed =
        !previousActive || previousOwnerSlot != slot || previousPlayer != player || affectedChanged;

    const CaughtStunOwnerState result = publicState(state, 0, 0);
    recordDecision(label, result, changed);
    return result;
}

CaughtStunOwnerState updateCaughtStun(const char* label, fopAc_ac_c* enemy, int stunTimer,
                                      int cryTimer) {
    RetainedStunState* state = findState(enemy, false);
    if (state == nullptr) {
        CaughtStunOwnerState missing;
        missing.enemy = enemy;
        missing.enemyDebug = actorDebug(enemy);
        missing.stunTimer = stunTimer;
        missing.cryTimer = cryTimer;
        missing.reason = CaughtStunOwnerReason::Cleared;
        recordDecision(label, missing, false);
        return missing;
    }

    if (state->active && state->slot != PlayerSlot::Invalid &&
        getPlayer(state->slot) != state->playerActor)
    {
        state->active = false;
        state->slot = PlayerSlot::Invalid;
        state->player = nullptr;
        state->playerActor = nullptr;
        clearAffectedPlayers(state);
        state->reason = CaughtStunOwnerReason::Cleared;
    }

    for (int i = 0; i < state->affectedCount;) {
        PlayerSlot slot = state->affectedSlots[i];
        if (!isValidSlot(slot) || getPlayer(slot) != state->affectedLocalActors[i]) {
            for (int j = i; j < state->affectedCount - 1; j++) {
                state->affectedSlots[j] = state->affectedSlots[j + 1];
                state->affectedLocalActors[j] = state->affectedLocalActors[j + 1];
            }
            state->affectedSlots[state->affectedCount - 1] = PlayerSlot::Invalid;
            state->affectedLocalActors[state->affectedCount - 1] = nullptr;
            state->affectedCount--;
        } else {
            i++;
        }
    }

    const CaughtStunOwnerState result = publicState(state, stunTimer, cryTimer);
    recordDecision(label, result, false);
    return result;
}

void clearCaughtStun(const char* label, fopAc_ac_c* enemy) {
    RetainedStunState* state = findState(enemy, false);
    if (state == nullptr) {
        return;
    }

    state->active = false;
    state->slot = PlayerSlot::Invalid;
    state->player = nullptr;
    state->playerActor = nullptr;
    clearAffectedPlayers(state);
    state->reason = CaughtStunOwnerReason::Cleared;
    const CaughtStunOwnerState result = publicState(state, 0, 0);
    recordDecision(label, result, true);
}

CaughtStunOwnerState getCaughtStun(fopAc_ac_c* enemy) {
    return publicState(findState(enemy, false), 0, 0);
}

const CaughtStunOwnerDebugState& getCaughtStunOwnerDebugState() {
    return s_debugState;
}

const char* caughtStunOwnerReasonName(CaughtStunOwnerReason reason) {
    switch (reason) {
    case CaughtStunOwnerReason::DirectPlayer:
        return "DirectPlayer";
    case CaughtStunOwnerReason::FallbackPrimary:
        return "FallbackPrimary";
    case CaughtStunOwnerReason::Cleared:
    default:
        return "Cleared";
    }
}

}  // namespace dusk::coop::caught_stun_owner
