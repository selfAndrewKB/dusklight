#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class fopAc_ac_c;

namespace dusk::coop {

// Co-op: plain snapshot of local controls for one player slot.
struct PlayerInputState {
    f32 stickValue = 0.0f;
    s16 stickAngle3D = 0;
    u32 triggerButtons = 0;
    u32 holdButtons = 0;
    u32 triggerLockR = 0;
    u32 holdLockR = 0;
};

PlayerInputState readLocalInput(PlayerSlot slot);
PlayerInputState readInputForActor(const fopAc_ac_c* actor);

}  // namespace dusk::coop
