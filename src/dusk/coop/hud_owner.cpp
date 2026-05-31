#include "dusk/coop/hud_owner.h"

#include "dusk/coop/camera.h"

namespace dusk::coop::hud_owner {
namespace {

PlayerSlot s_slot = PlayerSlot::Primary;
PlayerSlot s_slotStack[kPlayerSlotCount] = {};
int s_slotStackDepth = 0;

bool isSecondarySlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && slot != PlayerSlot::Primary &&
           static_cast<unsigned int>(slot) < kPlayerSlotCount;
}

}  // namespace

PlayerSlot currentSlot() {
    return s_slot;
}

void pushSlot(PlayerSlot slot) {
    if (s_slotStackDepth < kPlayerSlotCount) {
        s_slotStack[s_slotStackDepth++] = s_slot;
    }

    s_slot = slot;
}

void popSlot() {
    if (s_slotStackDepth > 0) {
        s_slot = s_slotStack[--s_slotStackDepth];
    } else {
        s_slot = PlayerSlot::Primary;
    }
}

bool isSecondaryPromptPass() {
    return camera::isSplitScreenEnabled() && isSecondarySlot(s_slot);
}

u8 buttonStatus(player_button_status::ButtonStatusKind kind) {
    return player_button_status::getStatus(s_slot, kind);
}

u8 buttonFlag(player_button_status::ButtonStatusKind kind) {
    return player_button_status::getFlag(s_slot, kind);
}

u8 threeDStatus() {
    return player_button_status::get3DStatus(s_slot);
}

u8 threeDDirection() {
    return player_button_status::get3DDirection(s_slot);
}

}  // namespace dusk::coop::hud_owner
