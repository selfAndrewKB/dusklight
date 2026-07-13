#pragma once

class dAttention_c;
class daAlink_c;
class fopAc_ac_c;
struct cXyz;

namespace dusk::coop::player_attention {

// Co-op: slot-local attention owner for ALINK gameplay that must not borrow P1's lock state.
dAttention_c* attentionForPlayer(daAlink_c* player);
dAttention_c* existingAttentionForSlot(int slot);
void updateForPlayer(daAlink_c* player);
void updateAdditionalPlayers();
bool isLockOn(daAlink_c* player);
fopAc_ac_c* zHintForPlayer(daAlink_c* player);
int requestZHintForPlayer(daAlink_c* player, fopAc_ac_c* actor, int priority);
void drawAll();
void drawForCamera(int cameraId);
bool isActorLockedByAnyPlayer(const fopAc_ac_c* actor);
bool isLockBlockedByPlayerStatus(dAttention_c* attention);
unsigned int attentionFlagsForOwner(dAttention_c* attention);
bool canSelectActor(dAttention_c* attention, const fopAc_ac_c* actor);
unsigned int actorFlagsForOwner(dAttention_c* attention, const fopAc_ac_c* actor);
const cXyz& actorPositionForOwner(dAttention_c* attention, const fopAc_ac_c* actor);
bool isViewportCursorDrawActive();
void setViewportCursorDrawList();
fopAc_ac_c* lockingPlayerForActor(const fopAc_ac_c* actor);

}  // namespace dusk::coop::player_attention
