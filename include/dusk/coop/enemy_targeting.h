#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_query.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop {

constexpr float kDefaultEnemyTargetRetainSeconds = 2.0f;

enum class EnemyTargetReason : u8 {
    AcquireNearest,
    RetainSticky,
    RetainCommitted,
    LostTarget,
    FallbackPrimary,
};

enum class EnemyTargetScope : u8 {
    Combat,
};

// Co-op: scope owns the retained target; mode only describes how one vanilla callsite may read or
// update that scope. This keeps one combat target per enemy while allowing non-sticky wake checks.
enum class EnemyTargetMode : u8 {
    StickyCombat,
    ImmediateAcquire,
};

struct EnemyTargetContext {
    fopAc_ac_c* observer = nullptr;
    EnemyTargetScope scope = EnemyTargetScope::Combat;
    EnemyTargetMode mode = EnemyTargetMode::StickyCombat;
    const char* label = nullptr;
    // Co-op: awareness callsites may restrict acquisition to players who pass the actor's
    // native visibility/eligibility rules. The chosen candidate still becomes the one Combat
    // owner; this is not an independent per-callsite target.
    PlayerQueryPredicate candidatePredicate = nullptr;
    void* candidatePredicateData = nullptr;
    bool committed = false;
    // Co-op: tune enemy target stickiness in simulation seconds, not render frames.
    float retainSeconds = kDefaultEnemyTargetRetainSeconds;
};

struct EnemyTargetResult {
    // Co-op: slot is the durable target identity. localActor is a process-local cache resolved
    // from that slot for original enemy code that needs position, distance, and angle. Future
    // network payloads should carry slot/scope/reason and re-resolve this pointer locally.
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* localActor = nullptr;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
    bool found = false;
    bool changed = false;
    EnemyTargetReason reason = EnemyTargetReason::LostTarget;
};

struct EnemyTargetDecisionDebug {
    EnemyTargetScope scope = EnemyTargetScope::Combat;
    char label[64] = {};
    fopAc_ac_c* observer = nullptr;
    PlayerQueryActorDebug observerDebug;
    EnemyTargetResult selected;
    PlayerQueryActorDebug selectedActorDebug;
    PlayerQueryResult nearest;
    PlayerQueryActorDebug nearestActorDebug;
    PlayerQueryCandidateDebug candidates[kPlayerSlotCount] = {};
    int candidateCount = 0;
    EnemyTargetReason reason = EnemyTargetReason::LostTarget;
    EnemyTargetMode mode = EnemyTargetMode::StickyCombat;
    bool committed = false;
    bool changed = false;
    bool retainedValid = false;
    bool retentionBlockedNearest = false;
    float retainSeconds = kDefaultEnemyTargetRetainSeconds;
    float stickyElapsedSeconds = 0.0f;
    u32 currentSimFrame = 0;
    u32 lastUpdatedSimFrame = 0;
};

struct EnemyTargetingDebugState {
    EnemyTargetDecisionDebug decisions[32] = {};
    int decisionCount = 0;
};

// Co-op: called once per game simulation tick so retention is independent of presentation FPS.
void advanceEnemyTargetingFrame(u32 frame);
EnemyTargetResult selectEnemyTarget(const EnemyTargetContext& context);
// Co-op: presentation and geometry consumers may inspect an existing target without running
// acquisition policy, advancing retention time, or emitting a new targeting decision.
EnemyTargetResult getEnemyTarget(fopAc_ac_c* observer, EnemyTargetScope scope);
void clearEnemyTarget(fopAc_ac_c* observer, EnemyTargetScope scope);
void clearAllEnemyTargets(fopAc_ac_c* observer);

const EnemyTargetingDebugState& getEnemyTargetingDebugState();
const char* enemyTargetReasonName(EnemyTargetReason reason);
const char* enemyTargetScopeName(EnemyTargetScope scope);
const char* enemyTargetModeName(EnemyTargetMode mode);

}  // namespace dusk::coop
