#pragma once

class dAttention_c;
class daAlink_c;

namespace dusk::coop::player_attention {

// Co-op: slot-local attention owner for ALINK gameplay that must not borrow P1's lock state.
dAttention_c* attentionForPlayer(daAlink_c* player);
void updateForPlayer(daAlink_c* player);
bool isLockOn(daAlink_c* player);

}  // namespace dusk::coop::player_attention
