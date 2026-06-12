#include "dusk/coop/bokoblin_attack_probe.h"

#include "f_op/f_op_actor.h"

#include <cstring>

namespace dusk::coop::bokoblin_attack_probe {
namespace {

constexpr int kProbeCount = 16;
constexpr u32 kLoopSuspectFrames = 90;

struct AttackRunState {
    uintptr_t actor = 0;
    int state = -1;
    int bck = -1;
    f32 animFrame = 0.0f;
    u32 startedFrame = 0;
    u32 firstHitFrame = 0;
    f32 firstHitAnimFrame = 0.0f;
    bool firstHitBeforeActiveWindow = false;
    bool firstHitGuarded = false;
};

BokoblinAttackProbeDebugState s_debugState;
static_assert(kProbeCount == 16, "Bokoblin probe arrays intentionally match debug state capacity");
BokoblinSteeringProbeDebugState s_steeringDebugState;
AttackRunState s_runs[kProbeCount] = {};
u32 s_currentSimFrame = 0;
u64 s_nextEventId = 1;
u64 s_nextSteeringEventId = 1;
int s_nextRunEvict = 0;
int s_nextProbeEvict = 0;
int s_nextSteeringEvict = 0;

AttackRunState* findRun(uintptr_t actor) {
    for (AttackRunState& run : s_runs) {
        if (run.actor == actor) {
            return &run;
        }
    }

    for (AttackRunState& run : s_runs) {
        if (run.actor == 0) {
            run.actor = actor;
            run.startedFrame = s_currentSimFrame;
            return &run;
        }
    }

    AttackRunState& run = s_runs[s_nextRunEvict++ % kProbeCount];
    run = {};
    run.actor = actor;
    run.startedFrame = s_currentSimFrame;
    return &run;
}

BokoblinAttackProbe* findProbe(uintptr_t actor) {
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

int speedSign(f32 speed) {
    if (speed > 0.05f) {
        return 1;
    }
    if (speed < -0.05f) {
        return -1;
    }
    return 0;
}

BokoblinSteeringProbe* findSteeringProbe(uintptr_t actor) {
    for (int i = 0; i < s_steeringDebugState.probeCount; i++) {
        if (s_steeringDebugState.probes[i].actor == actor) {
            return &s_steeringDebugState.probes[i];
        }
    }

    if (s_steeringDebugState.probeCount < kProbeCount) {
        return &s_steeringDebugState.probes[s_steeringDebugState.probeCount++];
    }

    return &s_steeringDebugState.probes[s_nextSteeringEvict++ % kProbeCount];
}

bool sameSemanticState(const BokoblinAttackProbe& lhs, const BokoblinAttackProbe& rhs) {
    return lhs.action == rhs.action && lhs.state == rhs.state && lhs.bck == rhs.bck &&
           lhs.targetSlot == rhs.targetSlot && lhs.targetFound == rhs.targetFound &&
           lhs.attackAnimationStarted == rhs.attackAnimationStarted &&
           lhs.attackActiveWindow == rhs.attackActiveWindow && lhs.guardedHit == rhs.guardedHit &&
           lhs.sphere0Hit == rhs.sphere0Hit && lhs.sphere1Hit == rhs.sphere1Hit &&
           lhs.preActiveWindowHit == rhs.preActiveWindowHit &&
           lhs.preActiveWindowGuarded == rhs.preActiveWindowGuarded &&
           lhs.firstHitBeforeActiveWindow == rhs.firstHitBeforeActiveWindow &&
           lhs.firstHitGuarded == rhs.firstHitGuarded &&
           lhs.sphere0.slot == rhs.sphere0.slot && lhs.sphere1.slot == rhs.sphere1.slot &&
           lhs.sphere0.guarded == rhs.sphere0.guarded &&
           lhs.sphere1.guarded == rhs.sphere1.guarded && lhs.loopSuspect == rhs.loopSuspect;
}

bool sameSteeringSemanticState(const BokoblinSteeringProbe& lhs,
                               const BokoblinSteeringProbe& rhs) {
    return lhs.action == rhs.action && lhs.state == rhs.state &&
           std::strcmp(lhs.label, rhs.label) == 0 && lhs.targetSlot == rhs.targetSlot &&
           lhs.targetFound == rhs.targetFound && lhs.nearestSlot == rhs.nearestSlot &&
           lhs.nearestFound == rhs.nearestFound && lhs.speedSign == rhs.speedSign &&
           lhs.detourActive == rhs.detourActive && lhs.moveOut == rhs.moveOut &&
           lhs.p1CloserThanTarget == rhs.p1CloserThanTarget;
}

}  // namespace

void advanceBokoblinAttackProbeFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
    s_steeringDebugState.currentSimFrame = frame;
}

void recordBokoblinAttackProbe(const BokoblinAttackProbe& probe) {
    if (probe.actor == 0) {
        return;
    }

    BokoblinAttackProbe next = probe;
    next.simFrame = s_currentSimFrame;

    AttackRunState* run = findRun(next.actor);
    if (run != nullptr) {
        const bool progressed = run->state != next.state || run->bck != next.bck ||
                                next.animFrame > run->animFrame + 0.001f;
        if (progressed) {
            run->state = next.state;
            run->bck = next.bck;
            run->animFrame = next.animFrame;
            run->startedFrame = s_currentSimFrame;
            if (!next.attackAnimationStarted || next.animFrame <= 0.001f) {
                run->firstHitFrame = 0;
                run->firstHitAnimFrame = 0.0f;
                run->firstHitBeforeActiveWindow = false;
                run->firstHitGuarded = false;
            }
        }

        const bool anyHit = next.sphere0Hit || next.sphere1Hit;
        const bool anyGuarded = next.guardedHit || next.sphere0.guarded || next.sphere1.guarded;
        next.preActiveWindowHit = anyHit && !next.attackActiveWindow;
        next.preActiveWindowGuarded = next.preActiveWindowHit && anyGuarded;
        if (anyHit && run->firstHitFrame == 0) {
            run->firstHitFrame = s_currentSimFrame;
            run->firstHitAnimFrame = next.animFrame;
            run->firstHitBeforeActiveWindow = !next.attackActiveWindow;
            run->firstHitGuarded = anyGuarded;
        }
        next.firstHitFrame = run->firstHitFrame;
        next.firstHitAnimFrame = run->firstHitAnimFrame;
        next.firstHitBeforeActiveWindow = run->firstHitBeforeActiveWindow;
        next.firstHitGuarded = run->firstHitGuarded;

        next.attackRunFrames = s_currentSimFrame - run->startedFrame;
        // Co-op: this is diagnostics only. It flags a Bokoblin that keeps re-entering attack
        // commitment without visible state/animation progress, which is the suspected guard-test loop.
        next.loopSuspect = !next.guardedHit && next.attackRunFrames >= kLoopSuspectFrames;
    }

    BokoblinAttackProbe* current = findProbe(next.actor);
    if (current == nullptr) {
        return;
    }

    const bool changed = current->eventId == 0 || !sameSemanticState(*current, next) ||
                         next.loopSuspect != current->loopSuspect;
    const u64 eventId = changed ? s_nextEventId++ : current->eventId;
    *current = next;
    current->eventId = eventId;
}

void recordBokoblinSteeringProbe(const BokoblinSteeringProbe& probe) {
    if (probe.actor == 0) {
        return;
    }

    BokoblinSteeringProbe next = probe;
    next.simFrame = s_currentSimFrame;
    next.speedSign = speedSign(next.speedF);

    BokoblinSteeringProbe* current = findSteeringProbe(next.actor);
    if (current == nullptr) {
        return;
    }

    const bool changed = current->eventId == 0 || !sameSteeringSemanticState(*current, next);
    const u64 eventId = changed ? s_nextSteeringEventId++ : current->eventId;
    *current = next;
    current->eventId = eventId;
}

void clearBokoblinAttackProbe(fopAc_ac_c* actor) {
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    if (actorPtr == 0) {
        return;
    }

    for (AttackRunState& run : s_runs) {
        if (run.actor == actorPtr) {
            run = {};
        }
    }

    for (int i = 0; i < s_debugState.probeCount; i++) {
        if (s_debugState.probes[i].actor == actorPtr) {
            s_debugState.probes[i] = {};
        }
    }

    for (int i = 0; i < s_steeringDebugState.probeCount; i++) {
        if (s_steeringDebugState.probes[i].actor == actorPtr) {
            s_steeringDebugState.probes[i] = {};
        }
    }
}

const BokoblinAttackProbeDebugState& getBokoblinAttackProbeDebugState() {
    return s_debugState;
}

const BokoblinSteeringProbeDebugState& getBokoblinSteeringProbeDebugState() {
    return s_steeringDebugState;
}

}  // namespace dusk::coop::bokoblin_attack_probe
