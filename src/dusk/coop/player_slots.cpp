#include "dusk/coop/player_slots.h"

#include "dusk/logging.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdint>

namespace dusk::coop {
namespace {

aurora::Module CoopLog("dusk::coop");

// Co-op: records actor identity only. Actor lifetime stays owned by the game.
fopAc_ac_c* s_players[kPlayerSlotCount] = {};

constexpr bool isValidSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Primary || slot == PlayerSlot::Secondary;
}

constexpr int slotIndex(PlayerSlot slot) {
    return static_cast<int>(slot);
}

}  // namespace

void registerPlayer(PlayerSlot slot, fopAc_ac_c* actor) {
    if (!isValidSlot(slot)) {
        return;
    }

    const int index = slotIndex(slot);
    fopAc_ac_c* previous_actor = s_players[index];
    s_players[index] = actor;

    if (previous_actor == actor) {
        CoopLog.debug("refreshed player slot {} actor 0x{:x}", index,
                      reinterpret_cast<uintptr_t>(actor));
    } else if (previous_actor != nullptr) {
        CoopLog.debug("replaced player slot {} actor 0x{:x} -> 0x{:x}", index,
                      reinterpret_cast<uintptr_t>(previous_actor), reinterpret_cast<uintptr_t>(actor));
    } else {
        CoopLog.debug("registered player slot {} actor 0x{:x}", index,
                      reinterpret_cast<uintptr_t>(actor));
    }
}

void unregisterPlayer(PlayerSlot slot, const fopAc_ac_c* actor) {
    if (!isValidSlot(slot)) {
        return;
    }

    const int index = slotIndex(slot);
    fopAc_ac_c*& registered_actor = s_players[index];
    if (registered_actor == actor) {
        registered_actor = nullptr;
        CoopLog.debug("unregistered player slot {} actor 0x{:x}", index,
                      reinterpret_cast<uintptr_t>(actor));
    } else {
        CoopLog.debug("ignored unregister for player slot {} actor 0x{:x}; registered actor is 0x{:x}",
                      index, reinterpret_cast<uintptr_t>(actor),
                      reinterpret_cast<uintptr_t>(registered_actor));
    }
}

fopAc_ac_c* getPlayer(PlayerSlot slot) {
    if (!isValidSlot(slot)) {
        return nullptr;
    }

    return s_players[slotIndex(slot)];
}

fopAc_ac_c* getPrimaryPlayer() {
    return getPlayer(PlayerSlot::Primary);
}

bool isPrimaryPlayer(const fopAc_ac_c* actor) {
    return actor != nullptr && actor == getPrimaryPlayer();
}

PlayerSlot getSlotForActor(const fopAc_ac_c* actor) {
    if (actor == nullptr) {
        return PlayerSlot::Invalid;
    }

    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (s_players[i] == actor) {
            return static_cast<PlayerSlot>(i);
        }
    }

    return PlayerSlot::Invalid;
}

int getPadForSlot(PlayerSlot slot) {
    switch (slot) {
    case PlayerSlot::Primary:
        return PAD_1;
    case PlayerSlot::Secondary:
        return PAD_2;
    default:
        return PAD_1;
    }
}

}  // namespace dusk::coop
