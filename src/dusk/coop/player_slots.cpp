#include "dusk/coop/player_slots.h"

#include "dusk/coop/camera.h"
#include "dusk/coop/horse_owner.h"
#include "dusk/coop/player_item_selection.h"
#include "dusk/logging.h"
#include "d/actor/d_a_alink.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_node.h"
#include "f_pc/f_pc_name.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdint>

namespace dusk::coop {
namespace {

aurora::Module CoopLog("dusk::coop");

// Co-op: records actor identity only. Actor lifetime stays owned by the game.
fopAc_ac_c* s_players[kPlayerSlotCount] = {};
bool s_requestedPlayers[kPlayerSlotCount] = {};

constexpr bool isValidSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Slot0 || slot == PlayerSlot::Slot1 ||
           slot == PlayerSlot::Slot2 || slot == PlayerSlot::Slot3;
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

    if (slot != PlayerSlot::Primary) {
        player_item_selection::initializeSlot(slot);
    }

    if (slot == PlayerSlot::Slot1) {
        camera::syncSecondaryPlayerAssignment();
        camera::ensureSecondaryCamera();
    }

    if (slot != PlayerSlot::Primary) {
        horse_owner::ensureHorseForSlot(slot);
    }
}

void unregisterPlayer(PlayerSlot slot, const fopAc_ac_c* actor) {
    if (!isValidSlot(slot)) {
        return;
    }

    const int index = slotIndex(slot);
    fopAc_ac_c*& registered_actor = s_players[index];
    if (registered_actor == actor) {
        if (slot != PlayerSlot::Primary) {
            horse_owner::releaseHorseForSlot(slot);
        }

        registered_actor = nullptr;
        CoopLog.debug("unregistered player slot {} actor 0x{:x}", index,
                      reinterpret_cast<uintptr_t>(actor));
        if (slot == PlayerSlot::Slot1) {
            camera::syncSecondaryPlayerAssignment();
        }
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

bool isPlayerInSlot(const fopAc_ac_c* actor, PlayerSlot slot) {
    return actor != nullptr && getSlotForActor(actor) == slot;
}

bool isSecondaryPlayer(const fopAc_ac_c* actor) {
    return isPlayerInSlot(actor, PlayerSlot::Slot1);
}

bool isAdditionalPlayer(const fopAc_ac_c* actor) {
    PlayerSlot slot = getSlotForActor(actor);
    return slot != PlayerSlot::Invalid && slot != PlayerSlot::Slot0;
}

bool isAdditionalPlayerSpawnRequest(const fopAc_ac_c* actor) {
    return getAdditionalPlayerSpawnRequestSlot(actor) != PlayerSlot::Invalid;
}

PlayerSlot getAdditionalPlayerSpawnRequestSlot(const fopAc_ac_c* actor) {
    if (actor == nullptr || actor->argument > kFirstAdditionalPlayerSpawnArgument) {
        return PlayerSlot::Invalid;
    }

    int slot = 1 + (kFirstAdditionalPlayerSpawnArgument - actor->argument);
    if (slot < 1 || slot >= kPlayerSlotCount) {
        return PlayerSlot::Invalid;
    }

    return static_cast<PlayerSlot>(slot);
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
    case PlayerSlot::Slot1:
        return PAD_2;
    case PlayerSlot::Slot2:
        return PAD_3;
    case PlayerSlot::Slot3:
        return PAD_4;
    default:
        return PAD_1;
    }
}

unsigned int spawnPlayer(PlayerSlot slot, daAlink_c* primary) {
    if (!isValidSlot(slot) || slot == PlayerSlot::Slot0 || primary == nullptr ||
        getPlayer(slot) != nullptr)
    {
        return 0;
    }

    // Co-op: requested additional slots are session intent and must be rebuilt after area loads.
    s_requestedPlayers[slotIndex(slot)] = true;

    cXyz pos = primary->current.pos;
    pos.x += 120.0f;
    csXyz angle = primary->shape_angle;

    layer_class* savedLayer = fpcLy_CurrentLayer();
    base_process_class* playScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e);
    if (playScene != nullptr) {
        fpcLy_SetCurrentLayer(&((process_node_class*)playScene)->layer);
    }

    const int spawnArgument = kFirstAdditionalPlayerSpawnArgument - (slotIndex(slot) - 1);

    // Co-op: encode the requested slot before ALINK create can register the actor in the sidecar.
    const unsigned int result = fopAcM_create(
        fpcNm_ALINK_e,
        fopAcM_GetParam(primary),
        &pos,
        primary->current.roomNo,
        &angle,
        nullptr,
        (s8)spawnArgument
    );

    fpcLy_SetCurrentLayer(savedLayer);
    return result;
}

void restoreRequestedPlayers(daAlink_c* primary) {
    if (primary == nullptr) {
        return;
    }

    for (int i = 1; i < kPlayerSlotCount; i++) {
        const PlayerSlot slot = static_cast<PlayerSlot>(i);
        if (s_requestedPlayers[i] && getPlayer(slot) == nullptr) {
            spawnPlayer(slot, primary);
        }
    }
}

bool isPlayerRequested(PlayerSlot slot) {
    return isValidSlot(slot) && s_requestedPlayers[slotIndex(slot)];
}

}  // namespace dusk::coop
