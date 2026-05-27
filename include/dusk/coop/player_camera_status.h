#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class daAlink_c;

namespace dusk::coop::player_camera_status {

u32 checkStatus0(PlayerSlot slot, u32 flag);
u32 checkStatus1(PlayerSlot slot, u32 flag);
u32 checkStatus0ForPlayerId(int playerId, u32 flag);
u32 checkStatus1ForPlayerId(int playerId, u32 flag);
u32 checkStatus0ForPlayer(const daAlink_c* player, u32 flag);
u32 checkStatus1ForPlayer(const daAlink_c* player, u32 flag);

void setStatus0(PlayerSlot slot, u32 flag);
void clearStatus0(PlayerSlot slot, u32 flag);
void setStatus1(PlayerSlot slot, u32 flag);
void clearStatus1(PlayerSlot slot, u32 flag);

void setStatus0ForPlayer(const daAlink_c* player, u32 flag);
void clearStatus0ForPlayer(const daAlink_c* player, u32 flag);
void setStatus1ForPlayer(const daAlink_c* player, u32 flag);
void clearStatus1ForPlayer(const daAlink_c* player, u32 flag);

}  // namespace dusk::coop::player_camera_status
