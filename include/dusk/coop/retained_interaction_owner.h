#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class daPy_py_c;
class fopAc_ac_c;

namespace dusk::coop::retained_interaction_owner {

enum class RetainedInteractionScope : u8 {
    Attach,
    Carry,
    Hang,
    Collect,
    Dig,
};

enum class RetainedInteractionReason : u8 {
    DirectPlayer,
    EnemyTarget,
    FallbackPrimary,
    Cleared,
};

struct RetainedInteractionState {
    fopAc_ac_c* owner = nullptr;
    RetainedInteractionScope scope = RetainedInteractionScope::Attach;
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* localPlayer = nullptr;
    fopAc_ac_c* localPlayerActor = nullptr;
    RetainedInteractionReason reason = RetainedInteractionReason::Cleared;
    bool active = false;
    bool found = false;
};

RetainedInteractionState beginRetainedInteraction(const char* label, fopAc_ac_c* owner,
                                                  RetainedInteractionScope scope,
                                                  fopAc_ac_c* playerActor,
                                                  RetainedInteractionReason reason);
RetainedInteractionState updateRetainedInteraction(const char* label, fopAc_ac_c* owner,
                                                   RetainedInteractionScope scope);
RetainedInteractionState getRetainedInteraction(fopAc_ac_c* owner,
                                                RetainedInteractionScope scope);
void clearRetainedInteraction(const char* label, fopAc_ac_c* owner,
                              RetainedInteractionScope scope);
void clearAllRetainedInteractions(fopAc_ac_c* owner);
int countRetainedInteractions(PlayerSlot slot, RetainedInteractionScope scope);

const char* retainedInteractionReasonName(RetainedInteractionReason reason);

}  // namespace dusk::coop::retained_interaction_owner
