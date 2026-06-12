#pragma once

#include "dusk/coop/player_slots.h"

class daPy_py_c;
class fopAc_ac_c;

namespace dusk::coop::event_owner {

PlayerSlot ownerSlotForActor(const fopAc_ac_c* fallbackActor);
daPy_py_c* ownerPlayerForActor(const fopAc_ac_c* fallbackActor);

PlayerSlot currentOwnerSlot();
daPy_py_c* currentOwnerPlayer();
bool isCurrentOwner(const fopAc_ac_c* actor);

int ownerPadForActor(const fopAc_ac_c* fallbackActor);
int currentOwnerPad();

}  // namespace dusk::coop::event_owner
