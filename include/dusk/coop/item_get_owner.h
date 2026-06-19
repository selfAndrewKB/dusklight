#pragma once

#include "dusk/coop/player_slots.h"

class daAlink_c;
class fopAc_ac_c;

namespace dusk::coop::item_get_owner {

void retainForEventSource(fopAc_ac_c* source, fopAc_ac_c* player);
void begin(daAlink_c* player);
void requestEnd();
void finishPendingEnd();
void end();
void reset();

bool isActive();
bool isDefaultGetItemEvent();
bool isDefaultGetItemEvent(const char* eventName);
bool shouldSkipAlinkStaff(const daAlink_c* player);
PlayerSlot currentSlot();
daAlink_c* currentPlayer();

}  // namespace dusk::coop::item_get_owner
