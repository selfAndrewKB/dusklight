#include "dusk/coop/ghost_rat_state_probe.h"

namespace dusk::coop::ghost_rat_state_probe {
namespace {

constexpr int kProbeCount = 24;

GhostRatStateProbeDebugState s_debugState;
u32 s_currentSimFrame = 0;
u64 s_nextEventId = 1;
int s_nextProbeEvict = 0;

GhostRatStateProbe* findProbe(uintptr_t actor) {
    for (int i = 0; i < s_debugState.probeCount; i++) {
        if (s_debugState.probes[i].actor == actor) {
            return &s_debugState.probes[i];
        }
    }

    if (s_debugState.probeCount < kProbeCount) {
        return &s_debugState.probes[s_debugState.probeCount++];
    }

    return &s_debugState.probes[s_nextProbeEvict++ % kProbeCount];
}

bool sameSemanticState(const GhostRatStateProbe& lhs, const GhostRatStateProbe& rhs) {
    return lhs.action == rhs.action && lhs.subAction == rhs.subAction && lhs.bck == rhs.bck &&
           lhs.wakeSlot == rhs.wakeSlot && lhs.wakeFound == rhs.wakeFound &&
           lhs.rangeGate == rhs.rangeGate && lhs.coneGate == rhs.coneGate &&
           lhs.bodySlotAvailable == rhs.bodySlotAvailable &&
           lhs.attackFrameGate == rhs.attackFrameGate && lhs.attackStarted == rhs.attackStarted &&
           lhs.switchNo == rhs.switchNo && lhs.switchGateActive == rhs.switchGateActive &&
           lhs.switchOn == rhs.switchOn && lhs.reachedAction == rhs.reachedAction;
}

}  // namespace

void advanceGhostRatStateProbeFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

void recordGhostRatStateProbe(const GhostRatStateProbe& probe) {
    if (probe.actor == 0) {
        return;
    }

    GhostRatStateProbe next = probe;
    next.simFrame = s_currentSimFrame;

    GhostRatStateProbe* current = findProbe(next.actor);
    if (current == nullptr) {
        return;
    }

    const bool changed = current->eventId == 0 || !sameSemanticState(*current, next);
    const u64 eventId = changed ? s_nextEventId++ : current->eventId;
    *current = next;
    current->eventId = eventId;
}

void clearGhostRatStateProbe(fopAc_ac_c* actor) {
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

const GhostRatStateProbeDebugState& getGhostRatStateProbeDebugState() {
    return s_debugState;
}

}  // namespace dusk::coop::ghost_rat_state_probe
