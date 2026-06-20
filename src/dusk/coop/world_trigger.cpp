#include "dusk/coop/world_trigger.h"

#include "dusk/coop/event_presentation.h"
#include "f_pc/f_pc_manager.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>

namespace dusk::coop::world_trigger {
namespace {

constexpr int kStateCount = 32;
constexpr int kDecisionCount = 32;
constexpr int kTransitionCount = 64;
constexpr int kLabelSize = 64;

struct RetainedState {
    TriggerState state;
    bool used = false;
};

RetainedState s_states[kStateCount];
WorldTriggerDebugState s_debugState;
int s_nextDecisionEvict = 0;
int s_nextTransitionEvict = 0;
u64 s_nextEventId = 1;
fopAc_ac_c* s_presentingSource = nullptr;
int s_presentingSourceId = -1;

void copyLabel(char* dst, const char* label) {
    std::snprintf(dst, kLabelSize, "%s", label != nullptr ? label : "");
}

bool actorIsLive(const fopAc_ac_c* actor, int actorId) {
    return actor != nullptr && actorId != -1 &&
           fpcM_SearchByID(actorId) ==
               static_cast<base_process_class*>(const_cast<fopAc_ac_c*>(actor));
}

bool stateIsLive(const TriggerState& state) {
    if (!state.active || !actorIsLive(state.source, state.sourceId)) {
        return false;
    }
    if (state.triggeringSlot == PlayerSlot::Invalid ||
        getPlayer(state.triggeringSlot) != state.triggeringPlayer)
    {
        return false;
    }
    return actorIsLive(state.triggeringPlayer, state.triggeringPlayerId);
}

RetainedState* findState(fopAc_ac_c* source) {
    if (source == nullptr) {
        return nullptr;
    }
    const int sourceId = static_cast<int>(fopAcM_GetID(source));
    for (RetainedState& retained : s_states) {
        if (retained.used && retained.state.source == source &&
            retained.state.sourceId == sourceId)
        {
            return &retained;
        }
    }
    return nullptr;
}

RetainedState* findStateByIdentity(fopAc_ac_c* source, int sourceId) {
    for (RetainedState& retained : s_states) {
        if (retained.used && retained.state.source == source &&
            retained.state.sourceId == sourceId)
        {
            return &retained;
        }
    }
    return nullptr;
}

RetainedState* allocateState(fopAc_ac_c* source) {
    for (RetainedState& retained : s_states) {
        if (!retained.used) {
            retained = {};
            retained.used = true;
            retained.state.source = source;
            retained.state.sourceId = static_cast<int>(fopAcM_GetID(source));
            return &retained;
        }
    }

    for (RetainedState& retained : s_states) {
        if (!retained.state.active) {
            retained = {};
            retained.used = true;
            retained.state.source = source;
            retained.state.sourceId = static_cast<int>(fopAcM_GetID(source));
            return &retained;
        }
    }

    return nullptr;
}

RetainedState* getOrCreateState(fopAc_ac_c* source) {
    RetainedState* retained = findState(source);
    return retained != nullptr ? retained : allocateState(source);
}

void recordDecision(fopAc_ac_c* source, const char* label, const TriggerPolicy& policy,
                    const TriggerMatch& match) {
    if (source == nullptr) {
        return;
    }

    const char* name = label != nullptr ? label : "";
    TriggerDecisionDebug* decision = nullptr;
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        TriggerDecisionDebug& candidate = s_debugState.decisions[i];
        if (candidate.source == reinterpret_cast<uintptr_t>(source) &&
            candidate.sourceId == static_cast<int>(fopAcM_GetID(source)) &&
            std::strcmp(candidate.label, name) == 0)
        {
            decision = &candidate;
            break;
        }
    }
    if (decision == nullptr) {
        if (s_debugState.decisionCount < kDecisionCount) {
            decision = &s_debugState.decisions[s_debugState.decisionCount++];
        } else {
            decision = &s_debugState.decisions[s_nextDecisionEvict++ % kDecisionCount];
        }
    }

    *decision = {};
    copyLabel(decision->label, name);
    decision->source = reinterpret_cast<uintptr_t>(source);
    decision->sourceId = static_cast<int>(fopAcM_GetID(source));
    decision->sourceProfile = static_cast<int>(fopAcM_GetProfName(source));
    decision->sourceRoom = static_cast<int>(fopAcM_GetRoomNo(source));
    decision->policy = policy;
    decision->selectedSlot = match.slot;
    decision->candidateMask = match.candidateMask;
    decision->eligibleMask = match.eligibleMask;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        decision->failureFlags[i] = match.failureFlags[i];
    }
    decision->found = match.found;
    s_debugState.revision++;
}

void recordTransition(const TriggerState& state, Transition transition, ReleaseReason reason) {
    TriggerTransitionDebug* record = nullptr;
    if (s_debugState.transitionCount < kTransitionCount) {
        record = &s_debugState.transitions[s_debugState.transitionCount++];
    } else {
        record = &s_debugState.transitions[s_nextTransitionEvict++ % kTransitionCount];
    }
    *record = {};
    record->eventId = s_nextEventId++;
    record->transition = transition;
    record->reason = reason;
    record->state = state;
    s_debugState.revision++;
}

void endPresentationInternal(RetainedState* retained) {
    if (retained == nullptr || !retained->state.presenting) {
        return;
    }

    retained->state.presenting = false;
    if (s_presentingSource == retained->state.source &&
        s_presentingSourceId == retained->state.sourceId)
    {
        s_presentingSource = nullptr;
        s_presentingSourceId = -1;
        event_presentation::end(event_presentation::Source::EnemyAuthoredDemo);
    }
}

void releaseInternal(RetainedState* retained, const char* label, ReleaseReason reason) {
    if (retained == nullptr || !retained->state.active) {
        return;
    }

    if (label != nullptr) {
        copyLabel(retained->state.label, label);
    }
    endPresentationInternal(retained);
    retained->state.active = false;
    recordTransition(retained->state, Transition::Release, reason);
}

void refreshDebugStates() {
    s_debugState.stateCount = 0;
    for (RetainedState& retained : s_states) {
        if (!retained.used) {
            continue;
        }
        if (retained.state.active && !stateIsLive(retained.state)) {
            releaseInternal(&retained, "world_trigger.stale", ReleaseReason::Cleared);
        }
        if (retained.state.active && s_debugState.stateCount < kStateCount) {
            s_debugState.states[s_debugState.stateCount++] = retained.state;
        }
    }
    for (int i = s_debugState.stateCount; i < kStateCount; i++) {
        s_debugState.states[i] = {};
    }
}

}  // namespace

TriggerMatch evaluate(fopAc_ac_c* source, const char* label, const TriggerPolicy& policy,
                      PlayerQueryPredicate predicate, void* userData) {
    TriggerMatch match;
    if (source == nullptr) {
        return match;
    }

    for (int i = 0; i < kPlayerSlotCount; i++) {
        const PlayerSlot slot = static_cast<PlayerSlot>(i);
        fopAc_ac_c* actor = getPlayer(slot);
        if (actor == nullptr) {
            continue;
        }

        const u8 bit = static_cast<u8>(1u << i);
        match.candidateMask |= bit;
        if (policy.activation == ActivationPolicy::PrimaryOnly &&
            slot != PlayerSlot::Primary)
        {
            match.failureFlags[i] = PlayerQueryEligibilityFailure_Status;
            continue;
        }

        const PlayerQueryEligibility eligibility =
            predicate != nullptr ? predicate(slot, actor, userData) : PlayerQueryEligibility{};
        match.failureFlags[i] = eligibility.failureFlags;
        if (!eligibility.eligible) {
            continue;
        }

        match.eligibleMask |= bit;
        if (!match.found) {
            // Co-op: slot order preserves vanilla P1 when multiple players qualify together.
            match.slot = slot;
            match.actor = actor;
            match.found = true;
        }
    }

    recordDecision(source, label, policy, match);
    return match;
}

void accept(fopAc_ac_c* source, const char* label, const TriggerPolicy& policy,
            const TriggerMatch& match, const TriggerMetadata& metadata) {
    if (source == nullptr || !match.found || match.actor == nullptr ||
        match.slot == PlayerSlot::Invalid)
    {
        return;
    }

    RetainedState* retained = getOrCreateState(source);
    if (retained == nullptr) {
        return;
    }
    if (retained->state.active) {
        if (retained->state.triggeringSlot == match.slot &&
            retained->state.triggeringPlayer == match.actor)
        {
            return;
        }
        releaseInternal(retained, label, ReleaseReason::Superseded);
    }

    retained->state = {};
    copyLabel(retained->state.label, label);
    retained->state.source = source;
    retained->state.sourceId = static_cast<int>(fopAcM_GetID(source));
    retained->state.sourceProfile = static_cast<int>(fopAcM_GetProfName(source));
    retained->state.sourceRoom = static_cast<int>(fopAcM_GetRoomNo(source));
    retained->state.triggeringPlayer = match.actor;
    retained->state.triggeringPlayerId = static_cast<int>(fopAcM_GetID(match.actor));
    retained->state.triggeringSlot = match.slot;
    retained->state.policy = policy;
    retained->state.metadata = metadata;
    retained->state.candidateMask = match.candidateMask;
    retained->state.eligibleMask = match.eligibleMask;
    retained->state.active = true;
    recordTransition(retained->state, Transition::Accept, ReleaseReason::Accepted);
}

void release(fopAc_ac_c* source, const char* label, ReleaseReason reason) {
    releaseInternal(findState(source), label, reason);
}

void clearSource(fopAc_ac_c* source, const char* label) {
    RetainedState* retained = findState(source);
    if (retained == nullptr) {
        return;
    }
    releaseInternal(retained, label, ReleaseReason::SourceDeleted);
    retained->used = false;
}

void clearPlayer(const fopAc_ac_c* player) {
    if (player == nullptr) {
        return;
    }
    for (RetainedState& retained : s_states) {
        if (retained.used && retained.state.active && retained.state.triggeringPlayer == player) {
            releaseInternal(&retained, "world_trigger.player_deleted", ReleaseReason::PlayerDeleted);
        }
    }
}

TriggerState stateForSource(fopAc_ac_c* source) {
    RetainedState* retained = findState(source);
    if (retained == nullptr || !retained->state.active) {
        return TriggerState{};
    }
    if (!stateIsLive(retained->state)) {
        releaseInternal(retained, "world_trigger.stale", ReleaseReason::Cleared);
        return TriggerState{};
    }
    return retained->state;
}

PlayerSlot subjectSlotForSource(fopAc_ac_c* source) {
    const TriggerState state = stateForSource(source);
    if (!state.active || state.policy.subject == SubjectPolicy::Primary) {
        return PlayerSlot::Primary;
    }
    return state.triggeringSlot;
}

fopAc_ac_c* subjectPlayerForSource(fopAc_ac_c* source) {
    return getPlayer(subjectSlotForSource(source));
}

bool beginPresentation(fopAc_ac_c* source, const char* label) {
    RetainedState* retained = findState(source);
    if (retained == nullptr || !retained->state.active || !stateIsLive(retained->state) ||
        retained->state.policy.presentation == PresentationPolicy::Split)
    {
        return false;
    }
    if (retained->state.presenting) {
        return true;
    }

    if (s_presentingSource != nullptr &&
        (s_presentingSource != source ||
         s_presentingSourceId != static_cast<int>(fopAcM_GetID(source))))
    {
        RetainedState* previous = findStateByIdentity(s_presentingSource, s_presentingSourceId);
        if (previous != nullptr) {
            endPresentationInternal(previous);
        } else {
            event_presentation::end(event_presentation::Source::EnemyAuthoredDemo);
            s_presentingSource = nullptr;
            s_presentingSourceId = -1;
        }
    }

    event_presentation::Options options;
    options.fullscreenSlot =
        retained->state.policy.presentation == PresentationPolicy::TriggeringPlayerFullscreen
            ? retained->state.triggeringSlot
            : PlayerSlot::Primary;
    options.hideNonPresenterVisuals = true;
    copyLabel(retained->state.label, label);
    retained->state.presenting = true;
    s_presentingSource = source;
    s_presentingSourceId = retained->state.sourceId;
    event_presentation::begin(event_presentation::Source::EnemyAuthoredDemo, options);
    s_debugState.revision++;
    return true;
}

void endPresentation(fopAc_ac_c* source, const char* label) {
    RetainedState* retained = findState(source);
    if (retained == nullptr) {
        return;
    }
    copyLabel(retained->state.label, label);
    endPresentationInternal(retained);
    s_debugState.revision++;
}

const WorldTriggerDebugState& getWorldTriggerDebugState() {
    refreshDebugStates();
    return s_debugState;
}

const char* triggerFamilyName(TriggerFamily family) {
    switch (family) {
    case TriggerFamily::SwitchArea:
        return "switch_area";
    case TriggerFamily::TagEvent:
        return "tag_event";
    case TriggerFamily::TagEvt:
        return "tag_evt";
    default:
        return "actor_local";
    }
}

const char* activationPolicyName(ActivationPolicy policy) {
    return policy == ActivationPolicy::AnyActivePlayer ? "any_active_player" : "primary_only";
}

const char* subjectPolicyName(SubjectPolicy policy) {
    return policy == SubjectPolicy::TriggeringPlayer ? "triggering_player" : "primary";
}

const char* presentationPolicyName(PresentationPolicy policy) {
    switch (policy) {
    case PresentationPolicy::PrimaryFullscreen:
        return "primary_fullscreen";
    case PresentationPolicy::TriggeringPlayerFullscreen:
        return "triggering_player_fullscreen";
    default:
        return "split";
    }
}

const char* transitionName(Transition transition) {
    return transition == Transition::Accept ? "accept" : "release";
}

const char* releaseReasonName(ReleaseReason reason) {
    switch (reason) {
    case ReleaseReason::Accepted:
        return "accepted";
    case ReleaseReason::EventEnded:
        return "event_ended";
    case ReleaseReason::CameraRestored:
        return "camera_restored";
    case ReleaseReason::SourceDeleted:
        return "source_deleted";
    case ReleaseReason::PlayerDeleted:
        return "player_deleted";
    case ReleaseReason::Superseded:
        return "superseded";
    default:
        return "cleared";
    }
}

}  // namespace dusk::coop::world_trigger
