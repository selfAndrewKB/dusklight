#include "dusk/coop/player_item_selection.h"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "d/d_save.h"

namespace dusk::coop::player_item_selection {
namespace {

constexpr int kSelectItemCount = 4;

struct SlotSelection {
    bool initialized = false;
    u8 selectItemIndex[kSelectItemCount] = {};
    u8 mixItemIndex[kSelectItemCount] = {};
};

SlotSelection s_selection[kPlayerSlotCount] = {};

bool isSecondarySlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && slot != PlayerSlot::Primary &&
           static_cast<unsigned int>(slot) < kPlayerSlotCount;
}

int slotIndex(PlayerSlot slot) {
    return static_cast<int>(slot);
}

bool isValidButton(int button) {
    return button >= 0 && button < kSelectItemCount;
}

PlayerSlot slotForPlayer(const daAlink_c* player) {
    PlayerSlot slot = getSlotForActor(static_cast<const fopAc_ac_c*>(player));
    return slot != PlayerSlot::Invalid ? slot : PlayerSlot::Primary;
}

u8 normalizeBottleItem(u8 item) {
    switch (item) {
    case dItemNo_OIL_BOTTLE_2_e:
        return dItemNo_OIL_BOTTLE_e;
    case dItemNo_RED_BOTTLE_2_e:
        return dItemNo_RED_BOTTLE_e;
    case dItemNo_OIL2_e:
        return dItemNo_OIL_e;
    default:
        return item;
    }
}

SlotSelection& getSecondarySelection(PlayerSlot slot) {
    SlotSelection& selection = s_selection[slotIndex(slot)];
    if (!selection.initialized) {
        initializeSlot(slot);
    }
    return selection;
}

u8 getSelectItemIndexForResolve(PlayerSlot slot, int button, bool initialize) {
    if (!isSecondarySlot(slot)) {
        return dComIfGs_getSelectItemIndex(button);
    }
    if (!isValidButton(button)) {
        return 0xff;
    }

    SlotSelection& selection = s_selection[slotIndex(slot)];
    if (!selection.initialized) {
        return initialize ? getSecondarySelection(slot).selectItemIndex[button]
                          : dComIfGs_getSelectItemIndex(button);
    }
    return selection.selectItemIndex[button];
}

u8 getMixItemIndexForResolve(PlayerSlot slot, int button, bool initialize) {
    if (!isSecondarySlot(slot)) {
        return dComIfGs_getMixItemIndex(button);
    }
    if (!isValidButton(button)) {
        return 0xff;
    }

    SlotSelection& selection = s_selection[slotIndex(slot)];
    if (!selection.initialized) {
        return initialize ? getSecondarySelection(slot).mixItemIndex[button]
                          : dComIfGs_getMixItemIndex(button);
    }
    return selection.mixItemIndex[button];
}

u8 getBombBagSlotForResolve(PlayerSlot slot, int button, bool initialize) {
    u8 itemSlot = getSelectItemIndexForResolve(slot, button, initialize);
    u8 mixSlot = getMixItemIndexForResolve(slot, button, initialize);

    if (itemSlot >= SLOT_15 && itemSlot < SLOT_18) {
        return itemSlot - SLOT_15;
    }
    if (mixSlot >= SLOT_15 && mixSlot < SLOT_18) {
        return mixSlot - SLOT_15;
    }
    return 0xff;
}

u8 resolveItem(PlayerSlot slot, int button, bool sanitize) {
    if (!isSecondarySlot(slot)) {
        return dComIfGp_getSelectItem(button);
    }
    if (!isValidButton(button)) {
        return dItemNo_NONE_e;
    }

    u8 itemSlot = getSelectItemIndexForResolve(slot, button, sanitize);
    if (button == SELECT_ITEM_DOWN) {
        return itemSlot;
    }
    if (itemSlot == 0xff) {
        return dItemNo_NONE_e;
    }

    u8 item = dComIfGs_getItem(itemSlot, false);
    if (item == dItemNo_NONE_e) {
        if (sanitize) {
            setSelectItemIndex(slot, button, 0xff);
        }
        return item;
    }

    u8 mixSlot = getMixItemIndexForResolve(slot, button, sanitize);
    if ((button == SELECT_ITEM_X || button == SELECT_ITEM_Y) && mixSlot != 0xff) {
        u8 mixItem = dComIfGs_getItem(mixSlot, false);
        if (mixItem == dItemNo_NONE_e) {
            if (sanitize) {
                setMixItemIndex(slot, button, 0xff);
            }
            return item;
        }

        if (mixItem == dItemNo_BOW_e) {
            mixItem = item;
            item = dItemNo_BOW_e;
        } else if (mixItem == dItemNo_FISHING_ROD_1_e) {
            mixItem = item;
            item = dItemNo_FISHING_ROD_1_e;
        }

        if (item == dItemNo_BOW_e) {
            switch (mixItem) {
            case dItemNo_NORMAL_BOMB_e:
            case dItemNo_WATER_BOMB_e:
            case dItemNo_POKE_BOMB_e:
                return dItemNo_BOMB_ARROW_e;
            case dItemNo_HAWK_EYE_e:
                return dItemNo_HAWK_ARROW_e;
            }
        } else if (item == dItemNo_FISHING_ROD_1_e) {
            switch (mixItem) {
            case dItemNo_BEE_CHILD_e:
                return dItemNo_BEE_ROD_e;
            case dItemNo_WORM_e:
                return dItemNo_WORM_ROD_e;
            case dItemNo_ZORAS_JEWEL_e:
                return dItemNo_JEWEL_ROD_e;
            }
        }
    }

    return item;
}

s16 resolveItemNum(PlayerSlot slot, int button, u8 item, bool initialize) {
    if (!isSecondarySlot(slot)) {
        return dComIfGp_getSelectItemNum(button);
    }

    if (item == dItemNo_NORMAL_BOMB_e || item == dItemNo_WATER_BOMB_e ||
        item == dItemNo_POKE_BOMB_e || item == dItemNo_BOMB_ARROW_e)
    {
        u8 bombBagSlot = getBombBagSlotForResolve(slot, button, initialize);
        return bombBagSlot != 0xff ? dComIfGs_getBombNum(bombBagSlot) : 0;
    }
    if (item == dItemNo_PACHINKO_e) {
        return dComIfGs_getPachinkoNum();
    }
    if (item == dItemNo_BEE_CHILD_e) {
        u8 itemSlot = getSelectItemIndexForResolve(slot, button, initialize);
        return itemSlot >= SLOT_11 && itemSlot < SLOT_15 ? dComIfGs_getBottleNum(itemSlot - SLOT_11)
                                                         : 0;
    }
    return 0;
}

int resolveItemMaxNum(PlayerSlot slot, u8 item) {
    if (item == dItemNo_BOMB_BAG_LV1_e) {
        return 1;
    }
    if (item == dItemNo_NORMAL_BOMB_e || item == dItemNo_WATER_BOMB_e ||
        item == dItemNo_POKE_BOMB_e || item == dItemNo_BOMB_ARROW_e)
    {
        return dComIfGs_getBombMax(item);
    }
    if (item == dItemNo_PACHINKO_e) {
        return dComIfGs_getPachinkoMax();
    }
    if (item == dItemNo_BEE_CHILD_e) {
        return dComIfGs_getBottleMax();
    }
    return 0;
}

}  // namespace

void initializeSlot(PlayerSlot slot) {
    if (!isSecondarySlot(slot)) {
        return;
    }

    SlotSelection& selection = s_selection[slotIndex(slot)];
    if (selection.initialized) {
        return;
    }

    for (int i = 0; i < kSelectItemCount; i++) {
        selection.selectItemIndex[i] = dComIfGs_getSelectItemIndex(i);
        selection.mixItemIndex[i] = dComIfGs_getMixItemIndex(i);
    }
    selection.initialized = true;
}

void resetSlot(PlayerSlot slot) {
    if (slot == PlayerSlot::Invalid || static_cast<unsigned int>(slot) >= kPlayerSlotCount) {
        return;
    }

    s_selection[slotIndex(slot)] = {};
}

u8 getSelectItemIndex(PlayerSlot slot, int button) {
    if (!isSecondarySlot(slot)) {
        return dComIfGs_getSelectItemIndex(button);
    }
    if (!isValidButton(button)) {
        return 0xff;
    }

    return getSecondarySelection(slot).selectItemIndex[button];
}

u8 getMixItemIndex(PlayerSlot slot, int button) {
    if (!isSecondarySlot(slot)) {
        return dComIfGs_getMixItemIndex(button);
    }
    if (!isValidButton(button)) {
        return 0xff;
    }

    return getSecondarySelection(slot).mixItemIndex[button];
}

void setSelectItemIndex(PlayerSlot slot, int button, u8 itemSlot) {
    if (!isSecondarySlot(slot)) {
        dComIfGs_setSelectItemIndex(button, itemSlot);
        return;
    }
    if (!isValidButton(button)) {
        return;
    }

    getSecondarySelection(slot).selectItemIndex[button] = itemSlot;
}

void setMixItemIndex(PlayerSlot slot, int button, u8 itemSlot) {
    if (!isSecondarySlot(slot)) {
        dComIfGs_setMixItemIndex(button, itemSlot);
        return;
    }
    if (!isValidButton(button)) {
        return;
    }

    getSecondarySelection(slot).mixItemIndex[button] = itemSlot;
}

u8 getItem(PlayerSlot slot, int button) {
    return resolveItem(slot, button, true);
}

s16 getItemNum(PlayerSlot slot, int button) {
    if (!isSecondarySlot(slot)) {
        return dComIfGp_getSelectItemNum(button);
    }

    return resolveItemNum(slot, button, getItem(slot, button), true);
}

int getItemMaxNum(PlayerSlot slot, int button) {
    if (!isSecondarySlot(slot)) {
        return dComIfGp_getSelectItemMaxNum(button);
    }

    return resolveItemMaxNum(slot, getItem(slot, button));
}

void setItemNum(PlayerSlot slot, int button, s16 value) {
    if (!isSecondarySlot(slot)) {
        dComIfGp_setSelectItemNum(button, value);
        return;
    }

    u8 item = getItem(slot, button);
    if (item == dItemNo_NORMAL_BOMB_e || item == dItemNo_WATER_BOMB_e ||
        item == dItemNo_POKE_BOMB_e || item == dItemNo_BOMB_ARROW_e)
    {
        u8 bombBagSlot = getBombBagSlotForResolve(slot, button, true);
        if (bombBagSlot != 0xff) {
            dComIfGs_setBombNum(bombBagSlot, value > dComIfGs_getBombMax(item)
                                                 ? dComIfGs_getBombMax(item)
                                                 : value);
        }
    } else if (item == dItemNo_PACHINKO_e) {
        dComIfGs_setPachinkoNum(value);
    } else if (item == dItemNo_BEE_CHILD_e) {
        u8 itemSlot = getSelectItemIndex(slot, button);
        if (itemSlot >= SLOT_11 && itemSlot < SLOT_15) {
            dComIfGs_setBottleNum(itemSlot - SLOT_11,
                                  value > dComIfGs_getBottleMax() ? dComIfGs_getBottleMax() : value);
        }
    }
}

void addItemNum(PlayerSlot slot, int button, s16 delta) {
    if (!isSecondarySlot(slot)) {
        dComIfGp_addSelectItemNum(button, delta);
        return;
    }

    u8 item = getItem(slot, button);
    if (item == dItemNo_NORMAL_BOMB_e || item == dItemNo_WATER_BOMB_e ||
        item == dItemNo_POKE_BOMB_e || item == dItemNo_BOMB_ARROW_e)
    {
        u8 bombBagSlot = getBombBagSlotForResolve(slot, button, true);
        if (bombBagSlot != 0xff) {
            dComIfGp_setItemBombNumCount(bombBagSlot, delta);
        }
    } else if (item == dItemNo_PACHINKO_e) {
        dComIfGp_setItemPachinkoNumCount(delta);
    } else if (item == dItemNo_BEE_CHILD_e) {
        u8 itemSlot = getSelectItemIndex(slot, button);
        if (itemSlot >= SLOT_11 && itemSlot < SLOT_15) {
            dComIfGs_addBottleNum(itemSlot - SLOT_11, delta);
        }
    }
}

ItemSelectionSnapshot inspectItem(PlayerSlot slot, int button) {
    u8 item = resolveItem(slot, button, false);
    return {
        getSelectItemIndexForResolve(slot, button, false),
        getMixItemIndexForResolve(slot, button, false),
        item,
        resolveItemNum(slot, button, item, false),
        isSecondarySlot(slot) ? resolveItemMaxNum(slot, item) : dComIfGp_getSelectItemMaxNum(button),
    };
}

u8 getItemForPlayer(const daAlink_c* player, int button) {
    return getItem(slotForPlayer(player), button);
}

s16 getItemNumForPlayer(const daAlink_c* player, int button) {
    return getItemNum(slotForPlayer(player), button);
}

int getItemMaxNumForPlayer(const daAlink_c* player, int button) {
    return getItemMaxNum(slotForPlayer(player), button);
}

void setItemNumForPlayer(const daAlink_c* player, int button, s16 value) {
    setItemNum(slotForPlayer(player), button, value);
}

void addItemNumForPlayer(const daAlink_c* player, int button, s16 delta) {
    addItemNum(slotForPlayer(player), button, delta);
}

void setBottleItemForPlayer(const daAlink_c* player, int button, u8 item) {
    PlayerSlot slot = slotForPlayer(player);
    if (!isSecondarySlot(slot)) {
        dComIfGs_setEquipBottleItemIn(button, item);
        return;
    }

    u8 itemSlot = getSelectItemIndex(slot, button);
    if (itemSlot < SLOT_11 || itemSlot >= SLOT_15) {
        return;
    }

    item = normalizeBottleItem(item);
    if (item == dItemNo_HOT_SPRING_e) {
        dMeter2Info_setHotSpringTimer(itemSlot);
    }
    dComIfGs_setItem(itemSlot, item);
    dComIfGp_setItem(itemSlot, item);
}

void emptyBottleForPlayer(const daAlink_c* player, int button) {
    setBottleItemForPlayer(player, button, dItemNo_EMPTY_BOTTLE_e);
}

}  // namespace dusk::coop::player_item_selection
