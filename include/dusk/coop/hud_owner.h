#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_button_status.h"
#include "dusk/coop/player_slots.h"

namespace dusk::coop::hud_owner {

struct ItemPresentation {
    u8 item;
    s16 count;
    int maxCount;
    bool showCount;
};

PlayerSlot currentSlot();
void pushSlot(PlayerSlot slot);
void popSlot();

bool isSecondaryPromptPass();
u8 buttonStatus(player_button_status::ButtonStatusKind kind);
u8 buttonFlag(player_button_status::ButtonStatusKind kind);
u8 threeDStatus();
u8 threeDDirection();
ItemPresentation itemPresentation(int button);
s16 horseLifeCount();
bool isHorseMeterVisible();

}  // namespace dusk::coop::hud_owner
