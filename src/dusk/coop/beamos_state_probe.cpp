#include "dusk/coop/beamos_state_probe.h"

namespace dusk::coop::beamos_state_probe {
namespace {

constexpr int kProbeCount = 16;

BeamosStateProbeDebugState s_debugState;
u32 s_currentSimFrame = 0;
u64 s_nextEventId = 1;
int s_nextEvict = 0;

BeamosStateProbe* findProbe(uintptr_t actor) {
    for (int i = 0; i < s_debugState.probeCount; i++) {
        if (s_debugState.probes[i].actor == actor) {
            return &s_debugState.probes[i];
        }
    }

    for (int i = 0; i < s_debugState.probeCount; i++) {
        if (s_debugState.probes[i].actor == 0) {
            return &s_debugState.probes[i];
        }
    }

    if (s_debugState.probeCount < kProbeCount) {
        return &s_debugState.probes[s_debugState.probeCount++];
    }

    return &s_debugState.probes[s_nextEvict++ % kProbeCount];
}

bool sameCandidateState(const BeamosWakeCandidate& lhs, const BeamosWakeCandidate& rhs) {
    return lhs.slot == rhs.slot && lhs.available == rhs.available &&
           lhs.eligible == rhs.eligible && lhs.failureFlags == rhs.failureFlags;
}

bool sameSemanticState(const BeamosStateProbe& lhs, const BeamosStateProbe& rhs) {
    if (lhs.action != rhs.action || lhs.mode != rhs.mode ||
        lhs.activationSwitchOn != rhs.activationSwitchOn ||
        lhs.eyeSwitchOn != rhs.eyeSwitchOn || lhs.bodySwitchOn != rhs.bodySwitchOn ||
        lhs.wakeCheckRan != rhs.wakeCheckRan || lhs.wakeFound != rhs.wakeFound ||
        lhs.wakeSlot != rhs.wakeSlot)
    {
        return false;
    }

    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (!sameCandidateState(lhs.candidates[i], rhs.candidates[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace

void advanceBeamosStateProbeFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

void recordBeamosState(const BeamosStateProbe& probe) {
    if (probe.actor == 0) {
        return;
    }

    BeamosStateProbe* current = findProbe(probe.actor);
    if (current == nullptr) {
        return;
    }

    BeamosStateProbe next = probe;
    next.simFrame = s_currentSimFrame;
    // Co-op: the actor lifecycle is sampled every tick, while wake eligibility is only valid
    // when the native warning state actually calls checkFindPlayer(). Preserve that last check.
    next.wakeCheckRan = current->wakeCheckRan;
    next.wakeFound = current->wakeFound;
    next.wakeSlot = current->wakeSlot;
    next.lastWakeCheckFrame = current->lastWakeCheckFrame;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        next.candidates[i] = current->candidates[i];
    }

    const bool changed = current->eventId == 0 || !sameSemanticState(*current, next);
    const u64 eventId = changed ? s_nextEventId++ : current->eventId;
    *current = next;
    current->eventId = eventId;
}

void recordBeamosWakeCheck(fopAc_ac_c* actor, bool found, PlayerSlot slot,
                           const BeamosWakeCandidate* candidates) {
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    if (actorPtr == 0 || candidates == nullptr) {
        return;
    }

    BeamosStateProbe* current = findProbe(actorPtr);
    if (current == nullptr) {
        return;
    }

    BeamosStateProbe next = *current;
    next.actor = actorPtr;
    next.simFrame = s_currentSimFrame;
    next.wakeCheckRan = true;
    next.wakeFound = found;
    next.wakeSlot = found ? slot : PlayerSlot::Invalid;
    next.lastWakeCheckFrame = s_currentSimFrame;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        next.candidates[i] = candidates[i];
    }

    const bool changed = current->eventId == 0 || !sameSemanticState(*current, next);
    const u64 eventId = changed ? s_nextEventId++ : current->eventId;
    *current = next;
    current->eventId = eventId;
}

void clearBeamosStateProbe(fopAc_ac_c* actor) {
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    if (actorPtr == 0) {
        return;
    }

    for (int i = 0; i < s_debugState.probeCount; i++) {
        if (s_debugState.probes[i].actor == actorPtr) {
            s_debugState.probes[i] = {};
        }
    }
}

const BeamosStateProbeDebugState& getBeamosStateProbeDebugState() {
    return s_debugState;
}

}  // namespace dusk::coop::beamos_state_probe
