#include "dusk/coop/hud_owner.h"

#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "dusk/coop/camera.h"
#include "dusk/coop/horse_owner.h"
#include "dusk/coop/player_item_selection.h"
#include "dusk/coop/ui_owner.h"

namespace dusk::coop::hud_owner {
namespace {

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

}  // namespace

PlayerSlot currentSlot() {
    return ui_owner::currentSlot();
}

void pushSlot(PlayerSlot slot) {
    ui_owner::pushPresentationSlot(slot);
}

void popSlot() {
    ui_owner::popPresentationSlot();
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
