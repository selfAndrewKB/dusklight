#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class daAlink_c;

namespace dusk::coop::player_item_selection {

struct ItemSelectionSnapshot {
    u8 selectIndex;
    u8 mixIndex;
    u8 item;
    s16 count;
    int maxCount;
};

void initializeSlot(PlayerSlot slot);
void resetSlot(PlayerSlot slot);

u8 getSelectItemIndex(PlayerSlot slot, int button);
u8 getMixItemIndex(PlayerSlot slot, int button);
void setSelectItemIndex(PlayerSlot slot, int button, u8 itemSlot);
void setMixItemIndex(PlayerSlot slot, int button, u8 itemSlot);

u8 getItem(PlayerSlot slot, int button);
s16 getItemNum(PlayerSlot slot, int button);
int getItemMaxNum(PlayerSlot slot, int button);
void setItemNum(PlayerSlot slot, int button, s16 value);
void addItemNum(PlayerSlot slot, int button, s16 delta);
ItemSelectionSnapshot inspectItem(PlayerSlot slot, int button);

u8 getItemForPlayer(const daAlink_c* player, int button);
s16 getItemNumForPlayer(const daAlink_c* player, int button);
int getItemMaxNumForPlayer(const daAlink_c* player, int button);
void setItemNumForPlayer(const daAlink_c* player, int button, s16 value);
void addItemNumForPlayer(const daAlink_c* player, int button, s16 delta);
void setBottleItemForPlayer(const daAlink_c* player, int button, u8 item);
void emptyBottleForPlayer(const daAlink_c* player, int button);

}  // namespace dusk::coop::player_item_selection
