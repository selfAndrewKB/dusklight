#include "dusk/coop/input.h"

#include "m_Do/m_Do_controller_pad.h"

namespace dusk::coop {

PlayerInputState readLocalInput(PlayerSlot slot) {
    const u32 pad = static_cast<u32>(getPadForSlot(slot));
    PlayerInputState input{};

    input.stickValue = mDoCPd_c::getStickValue(pad);
    input.stickAngle3D = mDoCPd_c::getStickAngle3D(pad);
    input.triggerButtons = mDoCPd_c::getTrig(pad);
    input.holdButtons = mDoCPd_c::getHold(pad);
    input.triggerLockR = mDoCPd_c::getTrigLockR(pad);
    input.holdLockR = mDoCPd_c::getHoldLockR(pad);

    return input;
}

PlayerInputState readInputForActor(const fopAc_ac_c* actor) {
    PlayerSlot slot = getSlotForActor(actor);
    if (slot == PlayerSlot::Invalid) {
        slot = PlayerSlot::Primary;
    }

    return readLocalInput(slot);
}

}  // namespace dusk::coop
