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

struct EnemyTargetContext {
    fopAc_ac_c* observer = nullptr;
    const char* system = nullptr;
    bool committed = false;
    // Co-op: tune enemy target stickiness in simulation seconds, not render frames.
    float retainSeconds = kDefaultEnemyTargetRetainSeconds;
};

struct EnemyTargetResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
    bool found = false;
    bool changed = false;
    EnemyTargetReason reason = EnemyTargetReason::LostTarget;
};

struct EnemyTargetDecisionDebug {
    char system[64] = {};
    fopAc_ac_c* observer = nullptr;
    PlayerQueryActorDebug observerDebug;
    EnemyTargetResult selected;
    PlayerQueryActorDebug selectedActorDebug;
    PlayerQueryCandidateDebug candidates[kPlayerSlotCount] = {};
    int candidateCount = 0;
    EnemyTargetReason reason = EnemyTargetReason::LostTarget;
    bool committed = false;
    bool changed = false;
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
void clearEnemyTarget(fopAc_ac_c* observer, const char* system);

const EnemyTargetingDebugState& getEnemyTargetingDebugState();
const char* enemyTargetReasonName(EnemyTargetReason reason);

}  // namespace dusk::coop
