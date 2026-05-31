#include "dusk/coop/hud_owner.h"

#include "dusk/coop/camera.h"
#include "dusk/coop/player_item_selection.h"
#include "dusk/coop/ui_owner.h"

namespace dusk::coop::hud_owner {
namespace {

bool isSecondarySlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && slot != PlayerSlot::Primary &&
           static_cast<unsigned int>(slot) < kPlayerSlotCount;
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
    return {
        player_item_selection::getItem(slot, button),
        player_item_selection::getItemNum(slot, button),
        player_item_selection::getItemMaxNum(slot, button),
    };
}

}  // namespace dusk::coop::hud_owner
