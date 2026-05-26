#pragma once

class dAttention_c;
class daAlink_c;
class fopAc_ac_c;

namespace dusk::coop::player_attention {

// Co-op: slot-local attention owner for ALINK gameplay that must not borrow P1's lock state.
dAttention_c* attentionForPlayer(daAlink_c* player);
dAttention_c* existingAttentionForSlot(int slot);
void updateForPlayer(daAlink_c* player);
bool isLockOn(daAlink_c* player);
void drawAll();
void drawForCamera(int cameraId);
bool isActorLockedByAnyPlayer(const fopAc_ac_c* actor);
bool isLockBlockedByPlayerStatus(dAttention_c* attention);
unsigned int attentionFlagsForOwner(dAttention_c* attention);
bool canSelectActor(dAttention_c* attention, const fopAc_ac_c* actor);
bool isViewportCursorDrawActive();
void setViewportCursorDrawList();

}  // namespace dusk::coop::player_attention
