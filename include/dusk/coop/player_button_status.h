#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class daAlink_c;

namespace dusk::coop::player_button_status {

enum class ButtonStatusKind {
    Do,
    A,
    R,
    Z,
};

u8 getStatus(PlayerSlot slot, ButtonStatusKind kind);
u8 getFlag(PlayerSlot slot, ButtonStatusKind kind);
void setStatus(PlayerSlot slot, ButtonStatusKind kind, u8 status, u8 flag);

u8 getStatusForPlayer(const daAlink_c* player, ButtonStatusKind kind);
u8 getFlagForPlayer(const daAlink_c* player, ButtonStatusKind kind);
void setStatusForPlayer(const daAlink_c* player, ButtonStatusKind kind, u8 status, u8 flag);

u8 get3DStatus(PlayerSlot slot);
u8 get3DDirection(PlayerSlot slot);
void set3DStatus(PlayerSlot slot, u8 status, u8 direction, u8 flag);

u8 get3DStatusForPlayer(const daAlink_c* player);
u8 get3DDirectionForPlayer(const daAlink_c* player);
void set3DStatusForPlayer(const daAlink_c* player, u8 status, u8 direction, u8 flag);

int messagePad();
bool messageTrigA();
bool messageTrigB();
bool messageHoldA();
bool messageHoldB();

}  // namespace dusk::coop::player_button_status
