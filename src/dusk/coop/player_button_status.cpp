#include "dusk/coop/player_button_status.h"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/event_owner.h"
#include "dusk/coop/message_owner.h"
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
    u8 xStatus = 0;
    u8 xFlag = 0;
    u8 yStatus = 0;
    u8 yFlag = 0;
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
    case ButtonStatusKind::X:
        return dComIfGp_getXStatus();
    case ButtonStatusKind::Y:
        return dComIfGp_getYStatus();
    }

    return 0;
}

u8 getPrimaryFlag(ButtonStatusKind kind) {
    u8 flag = 0;
    switch (kind) {
    case ButtonStatusKind::Do:
        flag |= dComIfGp_isDoSetFlag(1) ? 1 : 0;
        flag |= dComIfGp_isDoSetFlag(2) ? 2 : 0;
        flag |= dComIfGp_isDoSetFlag(4) ? 4 : 0;
        return flag;
    case ButtonStatusKind::A:
        flag |= dComIfGp_isASetFlag(1) ? 1 : 0;
        flag |= dComIfGp_isASetFlag(2) ? 2 : 0;
        flag |= dComIfGp_isASetFlag(4) ? 4 : 0;
        return flag;
    case ButtonStatusKind::R:
        flag |= dComIfGp_isRSetFlag(1) ? 1 : 0;
        flag |= dComIfGp_isRSetFlag(2) ? 2 : 0;
        flag |= dComIfGp_isRSetFlag(4) ? 4 : 0;
        return flag;
    case ButtonStatusKind::Z:
        flag |= dComIfGp_isZSetFlag(1) ? 1 : 0;
        flag |= dComIfGp_isZSetFlag(2) ? 2 : 0;
        flag |= dComIfGp_isZSetFlag(4) ? 4 : 0;
        return flag;
    case ButtonStatusKind::X:
        flag |= dComIfGp_isXSetFlag(1) ? 1 : 0;
        flag |= dComIfGp_isXSetFlag(2) ? 2 : 0;
        flag |= dComIfGp_isXSetFlag(4) ? 4 : 0;
        return flag;
    case ButtonStatusKind::Y:
        flag |= dComIfGp_isYSetFlag(1) ? 1 : 0;
        flag |= dComIfGp_isYSetFlag(2) ? 2 : 0;
        flag |= dComIfGp_isYSetFlag(4) ? 4 : 0;
        return flag;
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
    case ButtonStatusKind::X:
        dComIfGp_setXStatus(status, flag);
        break;
    case ButtonStatusKind::Y:
        dComIfGp_setYStatus(status, flag);
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
    case ButtonStatusKind::X:
        return status.xStatus;
    case ButtonStatusKind::Y:
        return status.yStatus;
    }

    return 0;
}

u8 getSecondaryFlag(PlayerSlot slot, ButtonStatusKind kind) {
    const SlotButtonStatus& status = s_status[slotIndex(slot)];
    switch (kind) {
    case ButtonStatusKind::Do:
        return status.doFlag;
    case ButtonStatusKind::A:
        return status.aFlag;
    case ButtonStatusKind::R:
        return status.rFlag;
    case ButtonStatusKind::Z:
        return status.zFlag;
    case ButtonStatusKind::X:
        return status.xFlag;
    case ButtonStatusKind::Y:
        return status.yFlag;
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
    case ButtonStatusKind::X:
        status.xStatus = value;
        status.xFlag = flag;
        break;
    case ButtonStatusKind::Y:
        status.yStatus = value;
        status.yFlag = flag;
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

u8 getFlag(PlayerSlot slot, ButtonStatusKind kind) {
    if (!isSecondarySlot(slot)) {
        return getPrimaryFlag(kind);
    }

    return getSecondaryFlag(slot, kind);
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

u8 getFlagForPlayer(const daAlink_c* player, ButtonStatusKind kind) {
    return getFlag(slotForPlayer(player), kind);
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
    return message_owner::isActive() ? message_owner::currentPad() : event_owner::currentOwnerPad();
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
