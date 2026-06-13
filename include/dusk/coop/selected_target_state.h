#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/types.h"
#include "dusk/coop/enemy_targeting.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class daPy_py_c;
class fopAc_ac_c;

namespace dusk::coop::selected_target_state {

enum class SelectedTargetStateReason : u8 {
    EnemyTarget,
    FilteredNearest,
    InvalidTarget,
    NoMatch,
};

struct SelectedTargetState {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    daPy_py_c* player = nullptr;
    PlayerQueryActorDebug actorDebug;
    cXyz pos = cXyz::Zero;
    s16 shapeAngleY = 0;
    f32 speedF = 0.0f;
    int damageWaitTimer = 0;
    int cutType = -1;
    int cutCount = -1;
    bool cutActive = false;
    bool horseRide = false;
    bool damageWaiting = false;
    bool status0_0x100 = false;
    bool ironBallSubject = false;
    bool equipHeavyBoots = false;
    bool available = false;
};

struct SelectedTargetDecisionDebug {
    char label[64] = {};
    fopAc_ac_c* observer = nullptr;
    PlayerQueryActorDebug observerDebug;
    SelectedTargetState state;
    SelectedTargetStateReason reason = SelectedTargetStateReason::NoMatch;
    bool found = false;
};

struct SelectedTargetDebugState {
    SelectedTargetDecisionDebug decisions[32] = {};
    int decisionCount = 0;
};

using PlayerStatePredicate = bool (*)(const SelectedTargetState& state);

SelectedTargetState stateForSlot(PlayerSlot slot, fopAc_ac_c* actor);
// Snapshot only. Actor files that need diagnostics should record the returned state with
// recordSelectedTargetState() so observer and label ownership stay explicit.
SelectedTargetState stateForEnemyTarget(const EnemyTargetResult& target);
SelectedTargetState findNearestPlayerState(const fopAc_ac_c* observer, const char* label,
                                           PlayerStatePredicate predicate,
                                           f32 maxDistanceXZ = -1.0f);
void recordSelectedTargetState(const fopAc_ac_c* observer, const char* label,
                               const SelectedTargetState& state,
                               SelectedTargetStateReason reason);

const SelectedTargetDebugState& getSelectedTargetDebugState();
const char* selectedTargetStateReasonName(SelectedTargetStateReason reason);

}  // namespace dusk::coop::selected_target_state
