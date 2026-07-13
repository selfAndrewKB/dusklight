#include "dusk/coop/hud_owner.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "dusk/coop/camera.h"
#include "dusk/coop/horse_owner.h"
#include "dusk/coop/player_item_selection.h"
#include "dusk/coop/ui_owner.h"
#include "f_op/f_op_actor_mng.h"

namespace dusk::coop::hud_owner {
namespace {

struct SlotVisibility {
    daAlink_c* owner = nullptr;
    fpc_ProcID ownerId = fpcM_ERROR_PROCESS_ID_e;
    bool visible = true;
};

SlotVisibility s_visibility[kPlayerSlotCount];

bool isSecondarySlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && slot != PlayerSlot::Primary &&
           static_cast<unsigned int>(slot) < kPlayerSlotCount;
}

bool usesArrowCounter(u8 item) {
    return item == dItemNo_BOW_e || item == dItemNo_LIGHT_ARROW_e ||
           item == dItemNo_ARROW_LV1_e || item == dItemNo_ARROW_LV2_e ||
           item == dItemNo_ARROW_LV3_e || item == dItemNo_HAWK_ARROW_e;
}

bool usesSelectionCounter(u8 item) {
    return item == dItemNo_BOMB_BAG_LV1_e || item == dItemNo_NORMAL_BOMB_e ||
           item == dItemNo_WATER_BOMB_e || item == dItemNo_POKE_BOMB_e ||
           item == dItemNo_PACHINKO_e || item == dItemNo_BEE_CHILD_e;
}

SlotVisibility* visibilityForPlayer(daAlink_c* player) {
    const PlayerSlot slot = getSlotForActor(player);
    const int slotIndex = static_cast<int>(slot);
    if (slotIndex <= static_cast<int>(PlayerSlot::Primary) || slotIndex >= kPlayerSlotCount) {
        return nullptr;
    }

    SlotVisibility& state = s_visibility[slotIndex];
    const fpc_ProcID ownerId = fopAcM_GetID(player);
    if (state.owner != player || state.ownerId != ownerId) {
        state = SlotVisibility{};
        state.owner = player;
        state.ownerId = ownerId;
    }

    return &state;
}

}  // namespace

PlayerSlot currentSlot() {
    return ui_owner::currentSlot();
}

daAlink_c* currentPlayer() {
    fopAc_ac_c* player = getPlayer(currentSlot());
    if (player == nullptr) {
        player = dComIfGp_getPlayer(0);
    }

    return static_cast<daAlink_c*>(player);
}

void pushSlot(PlayerSlot slot) {
    ui_owner::pushPresentationSlot(slot);
}

void popSlot() {
    ui_owner::popPresentationSlot();
}

void setVisibleForPlayer(daAlink_c* player, bool visible) {
    SlotVisibility* state = visibilityForPlayer(player);
    if (state != nullptr) {
        state->visible = visible;
    } else if (visible) {
        dComIfGp_2dShowOn();
    } else {
        dComIfGp_2dShowOff();
    }
}

bool isVisible(PlayerSlot slot) {
    if (slot == PlayerSlot::Invalid || slot == PlayerSlot::Primary) {
        return dComIfGp_2dShowCheck();
    }

    daAlink_c* player = static_cast<daAlink_c*>(getPlayer(slot));
    SlotVisibility* state = visibilityForPlayer(player);
    return state == nullptr || state->visible;
}

bool isSecondaryPromptPass() {
    return camera::isSplitScreenEnabled() && isSecondarySlot(currentSlot());
}

u8 buttonStatus(player_button_status::ButtonStatusKind kind) {
    return player_button_status::getStatus(currentSlot(), kind);
}

u8 buttonFlag(player_button_status::ButtonStatusKind kind) {
    return player_button_status::getFlag(currentSlot(), kind);
}

u8 threeDStatus() {
    return player_button_status::get3DStatus(currentSlot());
}

u8 threeDDirection() {
    return player_button_status::get3DDirection(currentSlot());
}

ItemPresentation itemPresentation(int button) {
    PlayerSlot slot = currentSlot();
    u8 item = player_item_selection::getItem(slot, button);
    ItemPresentation presentation = {item, 0, 0, false};

    if (usesArrowCounter(item)) {
        presentation.count = dComIfGs_getArrowNum();
        presentation.maxCount = dComIfGs_getArrowMax();
        presentation.showCount = true;
    } else if (item == dItemNo_BOMB_ARROW_e) {
        presentation.count = player_item_selection::getItemNum(slot, button);
        presentation.maxCount = player_item_selection::getItemMaxNum(slot, button);
        if (presentation.count > dComIfGs_getArrowNum()) {
            presentation.count = dComIfGs_getArrowNum();
        }
        if (presentation.maxCount < dComIfGs_getArrowMax()) {
            presentation.maxCount = dComIfGs_getArrowMax();
        }
        presentation.showCount = true;
    } else if (usesSelectionCounter(item)) {
        presentation.count = player_item_selection::getItemNum(slot, button);
        presentation.maxCount = player_item_selection::getItemMaxNum(slot, button);
        presentation.showCount = true;
    }

    return presentation;
}

s16 horseLifeCount() {
    PlayerSlot slot = currentSlot();
    if (slot == PlayerSlot::Primary) {
        return dMeter2Info_getHorseLifeCount();
    }

    daHorse_c* horse = horse_owner::getHorse(slot);
    return horse != nullptr ? horse->getLashCount() : dMeter2Info_getHorseLifeCount();
}

bool isHorseMeterVisible() {
    return horse_owner::shouldPresentLashMeter(currentSlot());
}

}  // namespace dusk::coop::hud_owner
