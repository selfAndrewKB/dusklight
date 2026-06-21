#pragma once

#include "dusk/coop/player_query.h"

class fopAc_ac_c;

namespace dusk::coop::player_sense {

bool isActive(PlayerSlot slot);
bool isRevealReady(PlayerSlot slot);
bool anyRevealReady();

PlayerQueryResult findNearestRevealPlayer(const fopAc_ac_c* observer, const char* system);

void registerRevealActor(const fopAc_ac_c* actor);
bool requiresSense(const fopAc_ac_c* actor);
bool canReveal(PlayerSlot slot, const fopAc_ac_c* actor);
void reset();

}  // namespace dusk::coop::player_sense
