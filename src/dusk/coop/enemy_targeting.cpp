#include "dusk/coop/enemy_targeting.h"

#include "dusk/game_clock.h"
#include "dusk/logging.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace dusk::coop {
namespace {

struct TargetState {
    fopAc_ac_c* observer = nullptr;
    EnemyTargetScope scope = EnemyTargetScope::Combat;
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    float stickyElapsedSeconds = 0.0f;
    u32 lastUpdatedSimFrame = 0;
};

aurora::Module CoopEnemyTargetingLog("dusk::coop.enemy_targeting");
std::vector<TargetState> s_states;
EnemyTargetingDebugState s_debugState;
u32 s_currentSimFrame = 0;

constexpr int kLabelSize = 64;
constexpr int kDebugDecisionCount = sizeof(s_debugState.decisions) / sizeof(s_debugState.decisions[0]);

void copyLabel(char* dst, const char* label) {
    std::snprintf(dst, kLabelSize, "%s", label != nullptr ? label : "");
}

TargetState* findState(fopAc_ac_c* observer, EnemyTargetScope scope, bool create) {
    for (TargetState& state : s_states) {
        if (state.observer == observer && state.scope == scope) {
            return &state;
        }
    }

    if (!create) {
        return nullptr;
    }

    try {
        TargetState state;
        state.observer = observer;
        state.scope = scope;
        state.lastUpdatedSimFrame = s_currentSimFrame;
        s_states.push_back(state);
    } catch (...) {
        CoopEnemyTargetingLog.warn("failed to allocate enemy target state for observer {}",
                                   static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(observer)));
        return nullptr;
    }
    return &s_states.back();
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

const PlayerQueryDecisionDebug* findPlayerQueryDecision(const fopAc_ac_c* observer, const char* label) {
    const PlayerQueryDebugState& queryDebug = getPlayerQueryDebugState();
    const char* labelName = label != nullptr ? label : "";
    for (int i = 0; i < queryDebug.decisionCount; i++) {
        const PlayerQueryDecisionDebug& decision = queryDebug.decisions[i];
        if (decision.observer == observer && std::strcmp(decision.system, labelName) == 0) {
            return &decision;
        }
    }
    return nullptr;
}

void recordDecision(const EnemyTargetContext& context, const EnemyTargetResult& result,
                    const PlayerQueryResult& nearest, const TargetState& state,
                    bool retainedValid) {
    EnemyTargetDecisionDebug* decision = nullptr;
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        if (s_debugState.decisions[i].observer == context.observer &&
            s_debugState.decisions[i].scope == context.scope)
        {
            decision = &s_debugState.decisions[i];
            break;
        }
    }
    if (decision == nullptr) {
        if (s_debugState.decisionCount < kDebugDecisionCount) {
            decision = &s_debugState.decisions[s_debugState.decisionCount++];
        } else {
            // Evict the oldest entry by simulation frame rather than silently clobbering slot 0.
            CoopEnemyTargetingLog.warn("enemy targeting debug buffer full ({} entries); evicting oldest",
                                       kDebugDecisionCount);
            decision = &s_debugState.decisions[0];
            for (int i = 1; i < kDebugDecisionCount; i++) {
                if (s_debugState.decisions[i].lastUpdatedSimFrame < decision->lastUpdatedSimFrame) {
                    decision = &s_debugState.decisions[i];
                }
            }
        }
    }

    decision->scope = context.scope;
    copyLabel(decision->label, context.label);
    decision->observer = context.observer;
    decision->observerDebug = actorDebug(context.observer);
    decision->selected = result;
    decision->selectedActorDebug = actorDebug(result.localActor);
    decision->nearest = nearest;
    decision->nearestActorDebug = actorDebug(nearest.actor);
    decision->reason = result.reason;
    decision->mode = context.mode;
    decision->committed = context.committed;
    decision->changed = result.changed;
    decision->retainedValid = retainedValid;
    decision->retentionBlockedNearest =
        nearest.found && result.found && nearest.actor != result.localActor &&
        (result.reason == EnemyTargetReason::RetainSticky ||
         result.reason == EnemyTargetReason::RetainCommitted);
    decision->retainSeconds = context.retainSeconds;
    decision->stickyElapsedSeconds = state.stickyElapsedSeconds;
    decision->currentSimFrame = s_currentSimFrame;
    decision->lastUpdatedSimFrame = state.lastUpdatedSimFrame;

    const PlayerQueryDecisionDebug* queryDecision = findPlayerQueryDecision(context.observer, context.label);
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
    result.localActor = query.actor;
    result.distance = query.distance;
    result.distanceXZ = query.distanceXZ;
    result.angleY = query.angleY;
    result.found = query.found;
    result.reason = reason;
    result.changed = previous.slot != result.slot || previous.actor != result.localActor || !query.found;
    return result;
}

void updateStateFromResult(TargetState* state, const EnemyTargetResult& result) {
    if (result.found) {
        state->slot = result.slot;
        state->actor = result.localActor;
    } else {
        state->slot = PlayerSlot::Invalid;
        state->actor = nullptr;
    }
}

void clearDebugDecision(fopAc_ac_c* observer, EnemyTargetScope scope) {
    for (int i = 0; i < s_debugState.decisionCount;) {
        if (s_debugState.decisions[i].observer == observer && s_debugState.decisions[i].scope == scope) {
            for (int j = i + 1; j < s_debugState.decisionCount; j++) {
                s_debugState.decisions[j - 1] = s_debugState.decisions[j];
            }
            s_debugState.decisions[--s_debugState.decisionCount] = EnemyTargetDecisionDebug{};
            continue;
        }
        i++;
    }
}

void clearAllDebugDecisions(fopAc_ac_c* observer) {
    for (int i = 0; i < s_debugState.decisionCount;) {
        if (s_debugState.decisions[i].observer == observer) {
            for (int j = i + 1; j < s_debugState.decisionCount; j++) {
                s_debugState.decisions[j - 1] = s_debugState.decisions[j];
            }
            s_debugState.decisions[--s_debugState.decisionCount] = EnemyTargetDecisionDebug{};
            continue;
        }
        i++;
    }
}

}  // namespace

void advanceEnemyTargetingFrame(u32 frame) {
    s_currentSimFrame = frame;
}

EnemyTargetResult selectEnemyTarget(const EnemyTargetContext& context) {
    if (context.observer == nullptr) {
        return EnemyTargetResult{};
    }

    TargetState* state = findState(context.observer, context.scope, true);
    if (state == nullptr) {
        EnemyTargetResult fallback = targetResultFromQuery(
            resultForTarget(context.observer, PlayerSlot::Slot0, getPlayer(PlayerSlot::Slot0)),
            EnemyTargetReason::FallbackPrimary, TargetState{});
        return fallback;
    }

    const TargetState previous = *state;
    // Co-op: an actor can read the same behavior scope from several original callsites in one
    // tick. Only simulation frame deltas advance retention, so presentation FPS cannot change AI.
    const u32 frameDelta = s_currentSimFrame >= state->lastUpdatedSimFrame
                               ? s_currentSimFrame - state->lastUpdatedSimFrame
                               : 0;
    state->lastUpdatedSimFrame = s_currentSimFrame;
    if (!context.committed) {
        // Co-op: committed attack frames pause sticky aging instead of refreshing it. The target
        // stays stable through follow-through, then resumes from the pre-attack elapsed time so a
        // nearby player can be reconsidered promptly after the attack state ends.
        state->stickyElapsedSeconds += static_cast<float>(frameDelta) * game_clock::sim_pace();
    }

    const PlayerQueryResult nearest =
        context.candidatePredicate != nullptr
            ? findNearestPlayerMatching(context.observer, context.label, context.candidatePredicate,
                                        context.candidatePredicateData)
            : findNearestPlayer(context.observer, context.label);
    EnemyTargetResult result;
    const bool retainedValid = isValidTarget(state->slot, state->actor);
    const PlayerQueryResult retained =
        retainedValid ? resultForTarget(context.observer, state->slot, state->actor) : PlayerQueryResult{};
    if (context.committed && retainedValid) {
        result = targetResultFromQuery(retained, EnemyTargetReason::RetainCommitted, previous);
    } else if (context.mode == EnemyTargetMode::ImmediateAcquire && nearest.found) {
        // Co-op: awareness/wake checks ask "who can this enemy notice right now?" They still write
        // the Combat owner on acquisition, but they must not let old chase stickiness hide a closer
        // eligible player from vanilla sight gates such as player_distance < mPlayerRange.
        result = targetResultFromQuery(nearest, retainedValid && nearest.slot == state->slot &&
                                       nearest.actor == state->actor
                                           ? EnemyTargetReason::RetainSticky
                                           : EnemyTargetReason::AcquireNearest,
                                       previous);
        // Co-op: reset unconditionally — confirming the same target is still fresh information.
        // Without this, prolonged awareness (head-search loops) pre-drains the combat window
        // before the enemy has engaged, causing a spurious AcquireNearest on the first chase frame.
        state->stickyElapsedSeconds = 0.0f;
    } else if (retainedValid && state->stickyElapsedSeconds < context.retainSeconds &&
               !(context.mode == EnemyTargetMode::ImmediateAcquire &&
                 context.candidatePredicate != nullptr))
    {
        // Co-op: sticky combat is for continuity after the enemy has engaged. Chase, close-range
        // gates, retreat/re-engage, and attack setup should not flicker between players just
        // because nearest distance crosses back and forth by small amounts.
        // Unfiltered ImmediateAcquire may retain when there is no fresher candidate. A filtered
        // wake query may not: "no eligible candidate" is the native gate result for this tick, and
        // returning stale Combat ownership would bypass the range/cone/LOS predicate.
        result = targetResultFromQuery(retained, EnemyTargetReason::RetainSticky, previous);
    } else if (nearest.found) {
        // Co-op: when the sticky window expires but the nearest player is still the retained target,
        // refresh quietly instead of toggling diagnostics between acquire/retain every few seconds.
        const bool sameRetainedTarget = retainedValid && nearest.slot == state->slot &&
                                        nearest.actor == state->actor;
        result = targetResultFromQuery(nearest, sameRetainedTarget ? EnemyTargetReason::RetainSticky
                                                                   : EnemyTargetReason::AcquireNearest,
                                       previous);
        state->stickyElapsedSeconds = 0.0f;
    } else if (context.candidatePredicate == nullptr &&
               getPlayer(PlayerSlot::Slot0) != nullptr)
    {
        // Co-op: the vanilla-like P1 safety fallback is valid only for unrestricted selection.
        // A filtered awareness query with no eligible players must remain empty; otherwise an
        // occluded or out-of-cone P1 can mask a visible added player at the wake boundary.
        result = targetResultFromQuery(resultForTarget(context.observer, PlayerSlot::Slot0,
                                                       getPlayer(PlayerSlot::Slot0)),
                                       EnemyTargetReason::FallbackPrimary, previous);
        state->stickyElapsedSeconds = 0.0f;
    } else {
        result.reason = EnemyTargetReason::LostTarget;
        result.changed = previous.slot != PlayerSlot::Invalid || previous.actor != nullptr;
        state->stickyElapsedSeconds = 0.0f;
    }

    updateStateFromResult(state, result);

    recordDecision(context, result, nearest, *state, retainedValid);
    return result;
}

EnemyTargetResult getEnemyTarget(fopAc_ac_c* observer, EnemyTargetScope scope) {
    if (observer == nullptr) {
        return EnemyTargetResult{};
    }

    TargetState* state = findState(observer, scope, false);
    if (state == nullptr || !isValidTarget(state->slot, state->actor)) {
        return EnemyTargetResult{};
    }

    const PlayerQueryResult current = resultForTarget(observer, state->slot, state->actor);
    EnemyTargetResult result;
    result.slot = current.slot;
    result.localActor = current.actor;
    result.distance = current.distance;
    result.distanceXZ = current.distanceXZ;
    result.angleY = current.angleY;
    result.found = current.found;
    result.reason = EnemyTargetReason::RetainSticky;
    return result;
}

void clearEnemyTarget(fopAc_ac_c* observer, EnemyTargetScope scope) {
    for (auto it = s_states.begin(); it != s_states.end(); ++it) {
        if (it->observer == observer && it->scope == scope) {
            s_states.erase(it);
            break;
        }
    }
    clearDebugDecision(observer, scope);
}

void clearAllEnemyTargets(fopAc_ac_c* observer) {
    for (auto it = s_states.begin(); it != s_states.end();) {
        if (it->observer == observer) {
            it = s_states.erase(it);
        } else {
            ++it;
        }
    }
    clearAllDebugDecisions(observer);
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

const char* enemyTargetScopeName(EnemyTargetScope scope) {
    switch (scope) {
    case EnemyTargetScope::Combat:
        return "combat";
    default:
        return "";
    }
}

const char* enemyTargetModeName(EnemyTargetMode mode) {
    switch (mode) {
    case EnemyTargetMode::StickyCombat:
        return "sticky_combat";
    case EnemyTargetMode::ImmediateAcquire:
        return "immediate_acquire";
    default:
        return "";
    }
}

}  // namespace dusk::coop
