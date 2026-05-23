#include "dusk/coop/young_gohma_state_probe.h"

namespace dusk::coop::young_gohma_state_probe {
namespace {

constexpr int kProbeCount = 16;
constexpr u32 kLoopSuspectFrames = 90;

struct YoungGohmaRunState {
    uintptr_t actor = 0;
    int action = -1;
    int subAction = -1;
    int bck = -1;
    f32 animFrame = 0.0f;
    u32 startedFrame = 0;
};

YoungGohmaStateProbeDebugState s_debugState;
YoungGohmaRunState s_runs[kProbeCount] = {};
u32 s_currentSimFrame = 0;
u64 s_nextEventId = 1;
int s_nextRunEvict = 0;
int s_nextProbeEvict = 0;

YoungGohmaRunState* findRun(uintptr_t actor) {
    for (YoungGohmaRunState& run : s_runs) {
        if (run.actor == actor) {
            return &run;
        }
    }

    for (YoungGohmaRunState& run : s_runs) {
        if (run.actor == 0) {
            run.actor = actor;
            run.startedFrame = s_currentSimFrame;
            return &run;
        }
    }

    YoungGohmaRunState& run = s_runs[s_nextRunEvict++ % kProbeCount];
    run = {};
    run.actor = actor;
    run.startedFrame = s_currentSimFrame;
    return &run;
}

YoungGohmaStateProbe* findProbe(uintptr_t actor) {
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

bool sameSemanticState(const YoungGohmaStateProbe& lhs, const YoungGohmaStateProbe& rhs) {
    return lhs.action == rhs.action && lhs.subAction == rhs.subAction && lhs.bck == rhs.bck &&
           lhs.targetSlot == rhs.targetSlot && lhs.targetFound == rhs.targetFound &&
           lhs.rangeGate == rhs.rangeGate && lhs.angleGate == rhs.angleGate &&
           lhs.losClear == rhs.losClear && lhs.plCheck == rhs.plCheck &&
           lhs.attackColliderActive == rhs.attackColliderActive && lhs.loopSuspect == rhs.loopSuspect;
}

}  // namespace

void advanceYoungGohmaStateProbeFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

void recordYoungGohmaStateProbe(const YoungGohmaStateProbe& probe) {
    if (probe.actor == 0) {
        return;
    }

    YoungGohmaStateProbe next = probe;
    next.simFrame = s_currentSimFrame;

    YoungGohmaRunState* run = findRun(next.actor);
    if (run != nullptr) {
        const bool progressed = run->action != next.action || run->subAction != next.subAction ||
                                run->bck != next.bck || next.animFrame > run->animFrame + 0.001f;
        if (progressed) {
            run->action = next.action;
            run->subAction = next.subAction;
            run->bck = next.bck;
            run->animFrame = next.animFrame;
            run->startedFrame = s_currentSimFrame;
        }

        next.stateRunFrames = s_currentSimFrame - run->startedFrame;
        // Co-op: diagnostics only. This flags the exact case under investigation: target retained,
        // but native Young Gohma gates are not allowing attack/recognition to proceed.
        next.loopSuspect = next.targetFound && !next.plCheck &&
                           next.stateRunFrames >= kLoopSuspectFrames &&
                           (next.action == 0 || next.action == 1);
    }

    YoungGohmaStateProbe* current = findProbe(next.actor);
    if (current == nullptr) {
        return;
    }

    const bool changed = current->eventId == 0 || !sameSemanticState(*current, next);
    const u64 eventId = changed ? s_nextEventId++ : current->eventId;
    *current = next;
    current->eventId = eventId;
}

void clearYoungGohmaStateProbe(fopAc_ac_c* actor) {
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    if (actorPtr == 0) {
        return;
    }

    for (YoungGohmaRunState& run : s_runs) {
        if (run.actor == actorPtr) {
            run = {};
        }
    }

    for (int i = 0; i < s_debugState.probeCount; i++) {
        if (s_debugState.probes[i].actor == actorPtr) {
            s_debugState.probes[i] = {};
        }
    }
}

const YoungGohmaStateProbeDebugState& getYoungGohmaStateProbeDebugState() {
    return s_debugState;
}

}  // namespace dusk::coop::young_gohma_state_probe
