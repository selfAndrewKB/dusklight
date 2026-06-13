#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/player_slots.h"

class daPy_py_c;
class fopAc_ac_c;

namespace dusk::coop::item_awareness {

enum class ItemAwarenessReason : u8 {
    OwnedBoomerang,
    NoMatch,
};

struct ItemAwarenessResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* localPlayer = nullptr;
    fopAc_ac_c* localPlayerActor = nullptr;
    fopAc_ac_c* itemActor = nullptr;
    PlayerQueryActorDebug observerDebug;
    PlayerQueryActorDebug playerDebug;
    PlayerQueryActorDebug itemDebug;
    f32 distanceXZ = 0.0f;
    ItemAwarenessReason reason = ItemAwarenessReason::NoMatch;
    bool found = false;
};

struct ItemAwarenessDecisionDebug {
    u64 eventId = 0;
    u32 simFrame = 0;
    char label[64] = {};
    ItemAwarenessResult result;
};

struct ItemAwarenessDebugState {
    ItemAwarenessDecisionDebug decisions[32] = {};
    int decisionCount = 0;
    u32 currentSimFrame = 0;
};

void advanceItemAwarenessFrame(u32 frame);

ItemAwarenessResult findActiveBoomerang(const fopAc_ac_c* observer, const char* label);

const ItemAwarenessDebugState& getItemAwarenessDebugState();
const char* itemAwarenessReasonName(ItemAwarenessReason reason);

}  // namespace dusk::coop::item_awareness
