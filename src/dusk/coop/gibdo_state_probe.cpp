#include "dusk/coop/gibdo_state_probe.h"

#include "f_op/f_op_actor.h"

namespace dusk::coop::gibdo_state_probe {
namespace {

constexpr int kProbeCount = 16;
constexpr u32 kLoopSuspectFrames = 90;

struct GibdoRunState {
    uintptr_t actor = 0;
    int action = -1;
    int moveMode = -1;
    int bck = -1;
    f32 animFrame = 0.0f;
    u32 startedFrame = 0;
};

struct GibdoBckState {
    uintptr_t actor = 0;
    int bck = -1;
};

GibdoStateProbeDebugState s_debugState;
GibdoRunState s_runs[kProbeCount] = {};
GibdoBckState s_bcks[kProbeCount] = {};
u32 s_currentSimFrame = 0;
u64 s_nextEventId = 1;
int s_nextRunEvict = 0;
int s_nextBckEvict = 0;
int s_nextProbeEvict = 0;

GibdoRunState* findRun(uintptr_t actor) {
    for (GibdoRunState& run : s_runs) {
        if (run.actor == actor) {
            return &run;
        }
    }

    for (GibdoRunState& run : s_runs) {
        if (run.actor == 0) {
            run.actor = actor;
            run.startedFrame = s_currentSimFrame;
            return &run;
        }
    }

    GibdoRunState& run = s_runs[s_nextRunEvict++ % kProbeCount];
    run = {};
    run.actor = actor;
    run.startedFrame = s_currentSimFrame;
    return &run;
}

GibdoBckState* findBck(uintptr_t actor, bool create) {
    for (GibdoBckState& bck : s_bcks) {
        if (bck.actor == actor) {
            return &bck;
        }
    }

    if (!create) {
        return nullptr;
    }

    for (GibdoBckState& bck : s_bcks) {
        if (bck.actor == 0) {
            bck.actor = actor;
            return &bck;
        }
    }

    GibdoBckState& bck = s_bcks[s_nextBckEvict++ % kProbeCount];
    bck = {};
    bck.actor = actor;
    return &bck;
}

GibdoStateProbe* findProbe(uintptr_t actor) {
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

bool sameSemanticState(const GibdoStateProbe& lhs, const GibdoStateProbe& rhs) {
    return lhs.action == rhs.action && lhs.moveMode == rhs.moveMode && lhs.bck == rhs.bck &&
           lhs.targetSlot == rhs.targetSlot && lhs.targetFound == rhs.targetFound &&
           lhs.rangeGate == rhs.rangeGate && lhs.angleGate == rhs.angleGate &&
           lhs.losClear == rhs.losClear && lhs.delayGate == rhs.delayGate &&
           lhs.attackGate == rhs.attackGate && lhs.attackStart == rhs.attackStart &&
           lhs.screamOwnerActive == rhs.screamOwnerActive &&
           lhs.screamOwnerAttackStarted == rhs.screamOwnerAttackStarted &&
           lhs.cryOwner == rhs.cryOwner && lhs.loopSuspect == rhs.loopSuspect;
}

}  // namespace

void advanceGibdoStateProbeFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

void noteGibdoBck(fopAc_ac_c* actor, int bck) {
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    if (actorPtr == 0) {
        return;
    }

    GibdoBckState* state = findBck(actorPtr, true);
    if (state != nullptr) {
        state->bck = bck;
    }
}

int currentGibdoBck(fopAc_ac_c* actor) {
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    if (actorPtr == 0) {
        return -1;
    }

    const GibdoBckState* state = findBck(actorPtr, false);
    return state != nullptr ? state->bck : -1;
}

void recordGibdoStateProbe(const GibdoStateProbe& probe) {
    if (probe.actor == 0) {
        return;
    }

    GibdoStateProbe next = probe;
    next.simFrame = s_currentSimFrame;
    if (next.bck < 0) {
        next.bck = currentGibdoBck(reinterpret_cast<fopAc_ac_c*>(next.actor));
    }

    GibdoRunState* run = findRun(next.actor);
    if (run != nullptr) {
        const bool progressed = run->action != next.action || run->moveMode != next.moveMode ||
                                run->bck != next.bck || next.animFrame > run->animFrame + 0.001f;
        if (progressed) {
            run->action = next.action;
            run->moveMode = next.moveMode;
            run->bck = next.bck;
            run->animFrame = next.animFrame;
            run->startedFrame = s_currentSimFrame;
        }

        next.stateRunFrames = s_currentSimFrame - run->startedFrame;
        // Co-op: diagnostics only. This flags a Gibdo that has selected a target and entered
        // active chase/attack state, but the native gates are not progressing to a real hit/scream.
        next.loopSuspect = next.targetFound && !next.attackGate &&
                           next.stateRunFrames >= kLoopSuspectFrames &&
                           (next.action == 2 || next.action == 3);
    }

    GibdoStateProbe* current = findProbe(next.actor);
    if (current == nullptr) {
        return;
    }

    const bool changed = current->eventId == 0 || !sameSemanticState(*current, next);
    const u64 eventId = changed ? s_nextEventId++ : current->eventId;
    *current = next;
    current->eventId = eventId;
}

void clearGibdoStateProbe(fopAc_ac_c* actor) {
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    if (actorPtr == 0) {
        return;
    }

    for (GibdoRunState& run : s_runs) {
        if (run.actor == actorPtr) {
            run = {};
        }
    }

    for (GibdoBckState& bck : s_bcks) {
        if (bck.actor == actorPtr) {
            bck = {};
        }
    }

    for (int i = 0; i < s_debugState.probeCount; i++) {
        if (s_debugState.probes[i].actor == actorPtr) {
            s_debugState.probes[i] = {};
        }
    }
}

const GibdoStateProbeDebugState& getGibdoStateProbeDebugState() {
    return s_debugState;
}

}  // namespace dusk::coop::gibdo_state_probe
