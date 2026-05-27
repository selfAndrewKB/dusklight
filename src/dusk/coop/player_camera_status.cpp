#include "dusk/coop/player_camera_status.h"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"

namespace dusk::coop::player_camera_status {
namespace {

u32 s_status0[kPlayerSlotCount] = {};
u32 s_status1[kPlayerSlotCount] = {};

bool isValidSlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && static_cast<unsigned int>(slot) < kPlayerSlotCount;
}

int slotIndex(PlayerSlot slot) {
    return static_cast<int>(slot);
}

PlayerSlot slotForPlayerId(int playerId) {
    if (playerId < 0 || playerId >= kPlayerSlotCount) {
        return PlayerSlot::Primary;
    }

    return static_cast<PlayerSlot>(playerId);
}

PlayerSlot slotForPlayer(const daAlink_c* player) {
    PlayerSlot slot = getSlotForActor(static_cast<const fopAc_ac_c*>(player));
    if (slot == PlayerSlot::Invalid) {
        return PlayerSlot::Primary;
    }

    return slot;
}

}  // namespace

u32 checkStatus0(PlayerSlot slot, u32 flag) {
    if (!isValidSlot(slot) || slot == PlayerSlot::Primary) {
        return dComIfGp_checkPlayerStatus0(0, flag);
    }

    return s_status0[slotIndex(slot)] & flag;
}

u32 checkStatus1(PlayerSlot slot, u32 flag) {
    if (!isValidSlot(slot) || slot == PlayerSlot::Primary) {
        return dComIfGp_checkPlayerStatus1(0, flag);
    }

    return s_status1[slotIndex(slot)] & flag;
}

u32 checkStatus0ForPlayerId(int playerId, u32 flag) {
    return checkStatus0(slotForPlayerId(playerId), flag);
}

u32 checkStatus1ForPlayerId(int playerId, u32 flag) {
    return checkStatus1(slotForPlayerId(playerId), flag);
}

u32 checkStatus0ForPlayer(const daAlink_c* player, u32 flag) {
    return checkStatus0(slotForPlayer(player), flag);
}

u32 checkStatus1ForPlayer(const daAlink_c* player, u32 flag) {
    return checkStatus1(slotForPlayer(player), flag);
}

void setStatus0(PlayerSlot slot, u32 flag) {
    if (!isValidSlot(slot) || slot == PlayerSlot::Primary) {
        dComIfGp_setPlayerStatus0(0, flag);
        return;
    }

    s_status0[slotIndex(slot)] |= flag;
}

void clearStatus0(PlayerSlot slot, u32 flag) {
    if (!isValidSlot(slot) || slot == PlayerSlot::Primary) {
        dComIfGp_clearPlayerStatus0(0, flag);
        return;
    }

    s_status0[slotIndex(slot)] &= ~flag;
}

void setStatus1(PlayerSlot slot, u32 flag) {
    if (!isValidSlot(slot) || slot == PlayerSlot::Primary) {
        dComIfGp_setPlayerStatus1(0, flag);
        return;
    }

    s_status1[slotIndex(slot)] |= flag;
}

void clearStatus1(PlayerSlot slot, u32 flag) {
    if (!isValidSlot(slot) || slot == PlayerSlot::Primary) {
        dComIfGp_clearPlayerStatus1(0, flag);
        return;
    }

    s_status1[slotIndex(slot)] &= ~flag;
}

void setStatus0ForPlayer(const daAlink_c* player, u32 flag) {
    setStatus0(slotForPlayer(player), flag);
}

void clearStatus0ForPlayer(const daAlink_c* player, u32 flag) {
    clearStatus0(slotForPlayer(player), flag);
}

void setStatus1ForPlayer(const daAlink_c* player, u32 flag) {
    setStatus1(slotForPlayer(player), flag);
}

void clearStatus1ForPlayer(const daAlink_c* player, u32 flag) {
    clearStatus1(slotForPlayer(player), flag);
}

}  // namespace dusk::coop::player_camera_status
