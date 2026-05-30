#include "dusk/coop/player_button_status.h"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/event_owner.h"
#include "m_Do/m_Do_controller_pad.h"

namespace dusk::coop::player_button_status {
namespace {

struct SlotButtonStatus {
    u8 doStatus = 0;
    u8 doFlag = 0;
    u8 aStatus = 0;
    u8 aFlag = 0;
    u8 rStatus = 0;
    u8 rFlag = 0;
    u8 zStatus = 0;
    u8 zFlag = 0;
    u8 threeDStatus = 0;
    u8 threeDDirection = 0;
    u8 threeDFlag = 0;
};

SlotButtonStatus s_status[kPlayerSlotCount] = {};

bool isSecondarySlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && slot != PlayerSlot::Primary &&
           static_cast<unsigned int>(slot) < kPlayerSlotCount;
}

int slotIndex(PlayerSlot slot) {
    return static_cast<int>(slot);
}

PlayerSlot slotForPlayer(const daAlink_c* player) {
    PlayerSlot slot = getSlotForActor(static_cast<const fopAc_ac_c*>(player));
    if (slot == PlayerSlot::Invalid) {
        return PlayerSlot::Primary;
    }

    return slot;
}

u8 getPrimaryStatus(ButtonStatusKind kind) {
    switch (kind) {
    case ButtonStatusKind::Do:
        return dComIfGp_getDoStatus();
    case ButtonStatusKind::A:
        return dComIfGp_getAStatus();
    case ButtonStatusKind::R:
        return dComIfGp_getRStatus();
    case ButtonStatusKind::Z:
        return dComIfGp_getZStatus();
    }

    return 0;
}

void setPrimaryStatus(ButtonStatusKind kind, u8 status, u8 flag) {
    switch (kind) {
    case ButtonStatusKind::Do:
        dComIfGp_setDoStatus(status, flag);
        break;
    case ButtonStatusKind::A:
        dComIfGp_setAStatus(status, flag);
        break;
    case ButtonStatusKind::R:
        dComIfGp_setRStatus(status, flag);
        break;
    case ButtonStatusKind::Z:
        dComIfGp_setZStatus(status, flag);
        break;
    }
}

u8 getSecondaryStatus(PlayerSlot slot, ButtonStatusKind kind) {
    const SlotButtonStatus& status = s_status[slotIndex(slot)];
    switch (kind) {
    case ButtonStatusKind::Do:
        return status.doStatus;
    case ButtonStatusKind::A:
        return status.aStatus;
    case ButtonStatusKind::R:
        return status.rStatus;
    case ButtonStatusKind::Z:
        return status.zStatus;
    }

    return 0;
}

void setSecondaryStatus(PlayerSlot slot, ButtonStatusKind kind, u8 value, u8 flag) {
    SlotButtonStatus& status = s_status[slotIndex(slot)];
    switch (kind) {
    case ButtonStatusKind::Do:
        status.doStatus = value;
        status.doFlag = flag;
        break;
    case ButtonStatusKind::A:
        status.aStatus = value;
        status.aFlag = flag;
        break;
    case ButtonStatusKind::R:
        status.rStatus = value;
        status.rFlag = flag;
        break;
    case ButtonStatusKind::Z:
        status.zStatus = value;
        status.zFlag = flag;
        break;
    }
}

}  // namespace

u8 getStatus(PlayerSlot slot, ButtonStatusKind kind) {
    if (!isSecondarySlot(slot)) {
        return getPrimaryStatus(kind);
    }

    return getSecondaryStatus(slot, kind);
}

void setStatus(PlayerSlot slot, ButtonStatusKind kind, u8 status, u8 flag) {
    if (!isSecondarySlot(slot)) {
        setPrimaryStatus(kind, status, flag);
        return;
    }

    setSecondaryStatus(slot, kind, status, flag);
}

u8 getStatusForPlayer(const daAlink_c* player, ButtonStatusKind kind) {
    return getStatus(slotForPlayer(player), kind);
}

void setStatusForPlayer(const daAlink_c* player, ButtonStatusKind kind, u8 status, u8 flag) {
    setStatus(slotForPlayer(player), kind, status, flag);
}

u8 get3DStatus(PlayerSlot slot) {
    if (!isSecondarySlot(slot)) {
        return dComIfGp_get3DStatus();
    }

    return s_status[slotIndex(slot)].threeDStatus;
}

u8 get3DDirection(PlayerSlot slot) {
    if (!isSecondarySlot(slot)) {
        return dComIfGp_get3DDirection();
    }

    return s_status[slotIndex(slot)].threeDDirection;
}

void set3DStatus(PlayerSlot slot, u8 status, u8 direction, u8 flag) {
    if (!isSecondarySlot(slot)) {
        dComIfGp_set3DStatus(status, direction, flag);
        return;
    }

    SlotButtonStatus& slotStatus = s_status[slotIndex(slot)];
    slotStatus.threeDStatus = status;
    slotStatus.threeDDirection = direction;
    slotStatus.threeDFlag = flag;
}

u8 get3DStatusForPlayer(const daAlink_c* player) {
    return get3DStatus(slotForPlayer(player));
}

u8 get3DDirectionForPlayer(const daAlink_c* player) {
    return get3DDirection(slotForPlayer(player));
}

void set3DStatusForPlayer(const daAlink_c* player, u8 status, u8 direction, u8 flag) {
    set3DStatus(slotForPlayer(player), status, direction, flag);
}

int messagePad() {
    return event_owner::currentOwnerPad();
}

bool messageTrigA() {
    return mDoCPd_c::getTrigA(messagePad()) != 0;
}

bool messageTrigB() {
    return mDoCPd_c::getTrigB(messagePad()) != 0;
}

bool messageHoldA() {
    return mDoCPd_c::getHoldA(messagePad()) != 0;
}

bool messageHoldB() {
    return mDoCPd_c::getHoldB(messagePad()) != 0;
}

}  // namespace dusk::coop::player_button_status
