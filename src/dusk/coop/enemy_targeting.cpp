#include "dusk/coop/enemy_targeting.h"

#include "dusk/game_clock.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>

namespace dusk::coop {
namespace {

struct TargetState {
    fopAc_ac_c* observer = nullptr;
    char system[64] = {};
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    float stickyElapsedSeconds = 0.0f;
    u32 lastUpdatedSimFrame = 0;
    bool inUse = false;
};

TargetState s_states[64];
EnemyTargetingDebugState s_debugState;
u32 s_currentSimFrame = 0;

constexpr int kSystemNameSize = 64;
constexpr int kDebugDecisionCount = sizeof(s_debugState.decisions) / sizeof(s_debugState.decisions[0]);

void copySystemName(char* dst, const char* system) {
    std::snprintf(dst, kSystemNameSize, "%s", system != nullptr ? system : "");
}

TargetState* findState(fopAc_ac_c* observer, const char* system, bool create) {
    const char* systemName = system != nullptr ? system : "";
    TargetState* freeState = nullptr;
    for (TargetState& state : s_states) {
        if (state.inUse && state.observer == observer && std::strcmp(state.system, systemName) == 0) {
            return &state;
        }
        if (!state.inUse && freeState == nullptr) {
            freeState = &state;
        }
    }

    if (!create) {
        return nullptr;
    }

    TargetState* state = freeState != nullptr ? freeState : &s_states[0];
    *state = TargetState{};
    state->observer = observer;
    copySystemName(state->system, systemName);
    state->lastUpdatedSimFrame = s_currentSimFrame;
    state->inUse = true;
    return state;
}

bool isValidTarget(PlayerSlot slot, fopAc_ac_c* actor) {
    return slot != PlayerSlot::Invalid && actor != nullptr && getPlayer(slot) == actor;
}

PlayerQueryResult resultForTarget(const fopAc_ac_c* observer, PlayerSlot slot, fopAc_ac_c* actor) {
    PlayerQueryResult result;
    if (observer == nullptr || !isValidTarget(slot, actor)) {
        return result;
    }

    result.slot = slot;
    result.actor = actor;
    result.distance = fopAcM_searchActorDistance(observer, actor);
    result.distanceXZ = fopAcM_searchActorDistanceXZ(observer, actor);
    result.angleY = fopAcM_searchActorAngleY(observer, actor);
    result.found = true;
    return result;
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

const PlayerQueryDecisionDebug* findPlayerQueryDecision(const fopAc_ac_c* observer, const char* system) {
    const PlayerQueryDebugState& queryDebug = getPlayerQueryDebugState();
    const char* systemName = system != nullptr ? system : "";
    for (int i = 0; i < queryDebug.decisionCount; i++) {
        const PlayerQueryDecisionDebug& decision = queryDebug.decisions[i];
        if (decision.observer == observer && std::strcmp(decision.system, systemName) == 0) {
            return &decision;
        }
    }
    return nullptr;
}

void recordDecision(const EnemyTargetContext& context, const EnemyTargetResult& result,
                    const TargetState& state) {
    const char* systemName = context.system != nullptr ? context.system : "";
    EnemyTargetDecisionDebug* decision = nullptr;
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        if (s_debugState.decisions[i].observer == context.observer &&
            std::strcmp(s_debugState.decisions[i].system, systemName) == 0)
        {
            decision = &s_debugState.decisions[i];
            break;
        }
    }
    if (decision == nullptr) {
        if (s_debugState.decisionCount < kDebugDecisionCount) {
            decision = &s_debugState.decisions[s_debugState.decisionCount++];
        } else {
            decision = &s_debugState.decisions[0];
        }
    }

    copySystemName(decision->system, systemName);
    decision->observer = context.observer;
    decision->observerDebug = actorDebug(context.observer);
    decision->selected = result;
    decision->selectedActorDebug = actorDebug(result.actor);
    decision->reason = result.reason;
    decision->committed = context.committed;
    decision->changed = result.changed;
    decision->retainSeconds = context.retainSeconds;
    decision->stickyElapsedSeconds = state.stickyElapsedSeconds;
    decision->currentSimFrame = s_currentSimFrame;
    decision->lastUpdatedSimFrame = state.lastUpdatedSimFrame;

    const PlayerQueryDecisionDebug* queryDecision = findPlayerQueryDecision(context.observer, systemName);
    decision->candidateCount = 0;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        decision->candidates[i] = PlayerQueryCandidateDebug{};
    }
    if (queryDecision != nullptr) {
        decision->candidateCount = queryDecision->candidateCount;
        for (int i = 0; i < queryDecision->candidateCount && i < kPlayerSlotCount; i++) {
            decision->candidates[i] = queryDecision->candidates[i];
        }
    }
}

EnemyTargetResult targetResultFromQuery(const PlayerQueryResult& query, EnemyTargetReason reason,
                                        const TargetState& previous) {
    EnemyTargetResult result;
    result.slot = query.slot;
    result.actor = query.actor;
    result.distance = query.distance;
    result.distanceXZ = query.distanceXZ;
    result.angleY = query.angleY;
    result.found = query.found;
    result.reason = reason;
    result.changed = previous.slot != result.slot || previous.actor != result.actor || !query.found;
    return result;
}

}  // namespace

void advanceEnemyTargetingFrame(u32 frame) {
    s_currentSimFrame = frame;
}

EnemyTargetResult selectEnemyTarget(const EnemyTargetContext& context) {
    if (context.observer == nullptr) {
        return EnemyTargetResult{};
    }

    TargetState* state = findState(context.observer, context.system, true);
    const TargetState previous = *state;
    // Co-op: actors can call the same targeting system more than once in a tick; only simulation
    // frame deltas advance retention so uncapped/interpolated rendering cannot change AI timing.
    const u32 frameDelta = s_currentSimFrame >= state->lastUpdatedSimFrame
                               ? s_currentSimFrame - state->lastUpdatedSimFrame
                               : 0;
    state->lastUpdatedSimFrame = s_currentSimFrame;
    if (context.committed) {
        state->stickyElapsedSeconds = 0.0f;
    } else {
        state->stickyElapsedSeconds += static_cast<float>(frameDelta) * game_clock::sim_pace();
    }

    const PlayerQueryResult nearest = findNearestPlayer(context.observer, context.system);
    EnemyTargetResult result;
    const bool retainedValid = isValidTarget(state->slot, state->actor);
    if (context.committed && retainedValid) {
        result = targetResultFromQuery(resultForTarget(context.observer, state->slot, state->actor),
                                       EnemyTargetReason::RetainCommitted, previous);
    } else if (retainedValid && state->stickyElapsedSeconds < context.retainSeconds) {
        result = targetResultFromQuery(resultForTarget(context.observer, state->slot, state->actor),
                                       EnemyTargetReason::RetainSticky, previous);
    } else if (nearest.found) {
        // Co-op: when the sticky window expires but the nearest player is still the retained target,
        // refresh quietly instead of toggling diagnostics between acquire/retain every few seconds.
        const bool sameRetainedTarget = retainedValid && nearest.slot == state->slot &&
                                        nearest.actor == state->actor;
        result = targetResultFromQuery(nearest, sameRetainedTarget ? EnemyTargetReason::RetainSticky
                                                                   : EnemyTargetReason::AcquireNearest,
                                       previous);
        state->slot = nearest.slot;
        state->actor = nearest.actor;
        state->stickyElapsedSeconds = 0.0f;
    } else if (getPlayer(PlayerSlot::Slot0) != nullptr) {
        result = targetResultFromQuery(resultForTarget(context.observer, PlayerSlot::Slot0,
                                                       getPlayer(PlayerSlot::Slot0)),
                                       EnemyTargetReason::FallbackPrimary, previous);
        state->slot = result.slot;
        state->actor = result.actor;
        state->stickyElapsedSeconds = 0.0f;
    } else {
        result.reason = EnemyTargetReason::LostTarget;
        result.changed = previous.slot != PlayerSlot::Invalid || previous.actor != nullptr;
        state->slot = PlayerSlot::Invalid;
        state->actor = nullptr;
        state->stickyElapsedSeconds = 0.0f;
    }

    if (result.found) {
        state->slot = result.slot;
        state->actor = result.actor;
    }

    recordDecision(context, result, *state);
    return result;
}

void clearEnemyTarget(fopAc_ac_c* observer, const char* system) {
    TargetState* state = findState(observer, system, false);
    if (state != nullptr) {
        *state = TargetState{};
    }
}

const EnemyTargetingDebugState& getEnemyTargetingDebugState() {
    return s_debugState;
}

const char* enemyTargetReasonName(EnemyTargetReason reason) {
    switch (reason) {
    case EnemyTargetReason::AcquireNearest:
        return "AcquireNearest";
    case EnemyTargetReason::RetainSticky:
        return "RetainSticky";
    case EnemyTargetReason::RetainCommitted:
        return "RetainCommitted";
    case EnemyTargetReason::LostTarget:
        return "LostTarget";
    case EnemyTargetReason::FallbackPrimary:
        return "FallbackPrimary";
    default:
        return "";
    }
}

}  // namespace dusk::coop
