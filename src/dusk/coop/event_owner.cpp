#include "dusk/coop/event_owner.h"

#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"

namespace dusk::coop::event_owner {
namespace {

PlayerSlot normalizeSlot(PlayerSlot slot) {
    if (slot == PlayerSlot::Invalid) {
        return PlayerSlot::Primary;
    }

    return slot;
}

daPy_py_c* playerForSlot(PlayerSlot slot) {
    fopAc_ac_c* player = getPlayer(normalizeSlot(slot));
    if (player == nullptr) {
        player = dComIfGp_getPlayer(0);
    }

    return static_cast<daPy_py_c*>(player);
}

}  // namespace

PlayerSlot ownerSlotForActor(const fopAc_ac_c* fallbackActor) {
    // Co-op: accepted events already retain their requester in Pt1. Prefer that owner and
    // only fall back to the explicit actor/P1 path when no player requested the event.
    PlayerSlot slot = getSlotForActor(dComIfGp_event_getPt1());
    if (slot != PlayerSlot::Invalid) {
        return slot;
    }

    return normalizeSlot(getSlotForActor(fallbackActor));
}

daPy_py_c* ownerPlayerForActor(const fopAc_ac_c* fallbackActor) {
    return playerForSlot(ownerSlotForActor(fallbackActor));
}

PlayerSlot currentOwnerSlot() {
    return ownerSlotForActor(nullptr);
}

daPy_py_c* currentOwnerPlayer() {
    return ownerPlayerForActor(nullptr);
}

bool isCurrentOwner(const fopAc_ac_c* actor) {
    PlayerSlot slot = getSlotForActor(actor);
    if (slot == PlayerSlot::Invalid) {
        return false;
    }

    return slot == currentOwnerSlot();
}

int ownerPadForActor(const fopAc_ac_c* fallbackActor) {
    return getPadForSlot(ownerSlotForActor(fallbackActor));
}

int currentOwnerPad() {
    return getPadForSlot(currentOwnerSlot());
}

}  // namespace dusk::coop::event_owner
