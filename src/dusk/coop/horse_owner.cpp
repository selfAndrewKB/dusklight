#include "dusk/coop/horse_owner.h"

#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"
#include "dusk/frame_interpolation.h"
#include "dusk/logging.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"
#include "m_Do/m_Do_ext.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace dusk::coop::horse_owner {
namespace {

aurora::Module CoopHorseLog("dusk::coop::horse_owner");

struct HorseAnimationClone {
    J3DAnmTransform* source = nullptr;
    mDoExt_transAnmBas* localized = nullptr;
};

struct HorseSlotState {
    daHorse_c* horse = nullptr;
    fpc_ProcID pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
    std::vector<HorseAnimationClone> animations;
    HorseReinSimulationState reins;
};

HorseSlotState s_horses[kPlayerSlotCount];

constexpr bool isValidSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Slot0 || slot == PlayerSlot::Slot1 ||
           slot == PlayerSlot::Slot2 || slot == PlayerSlot::Slot3;
}

constexpr int slotIndex(PlayerSlot slot) {
    return static_cast<int>(slot);
}

HorseSlotState* stateForSlot(PlayerSlot slot) {
    if (!isValidSlot(slot)) {
        return nullptr;
    }

    return &s_horses[slotIndex(slot)];
}

const HorseSlotState* stateForSlotConst(PlayerSlot slot) {
    return stateForSlot(slot);
}

HorseSlotState* stateForHorse(const daHorse_c* horse) {
    PlayerSlot slot = getSlotForHorse(horse);
    if (slot == PlayerSlot::Invalid) {
        slot = getAdditionalHorseSpawnRequestSlot(horse);
    }

    return stateForSlot(slot);
}

void clearAnimations(HorseSlotState* state) {
    if (state == nullptr) {
        return;
    }

    for (HorseAnimationClone& animation : state->animations) {
        // Co-op: localized wrappers are JKR allocations and must leave through the JKR heap path.
        JKR_DELETE(animation.localized);
    }
    state->animations.clear();
}

void clearSlot(HorseSlotState* state) {
    if (state == nullptr) {
        return;
    }

    clearAnimations(state);
    state->horse = nullptr;
    state->pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
    state->reins = {};
}

}  // namespace

bool isAdditionalHorseSpawnRequest(const fopAc_ac_c* actor) {
    return getAdditionalHorseSpawnRequestSlot(actor) != PlayerSlot::Invalid;
}

PlayerSlot getAdditionalHorseSpawnRequestSlot(const fopAc_ac_c* actor) {
    if (actor == nullptr || actor->argument > kFirstAdditionalHorseSpawnArgument) {
        return PlayerSlot::Invalid;
    }

    const int slot = 1 + (kFirstAdditionalHorseSpawnArgument - actor->argument);
    if (slot < 1 || slot >= kPlayerSlotCount) {
        return PlayerSlot::Invalid;
    }

    return static_cast<PlayerSlot>(slot);
}

void registerHorse(PlayerSlot slot, daHorse_c* horse) {
    HorseSlotState* state = stateForSlot(slot);
    if (state == nullptr || horse == nullptr) {
        return;
    }

    if (state->horse != nullptr && state->horse != horse) {
        clearAnimations(state);
        state->reins = {};
    }

    state->horse = horse;
    state->pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
    CoopHorseLog.debug("registered horse slot {} actor 0x{:x}", slotIndex(slot),
                       reinterpret_cast<uintptr_t>(horse));

    if (slot != PlayerSlot::Primary && getPlayer(slot) == nullptr) {
        releaseHorseForSlot(slot);
    }
}

void unregisterHorse(PlayerSlot slot, const daHorse_c* horse) {
    HorseSlotState* state = stateForSlot(slot);
    if (state == nullptr || state->horse != horse) {
        return;
    }

    CoopHorseLog.debug("unregistered horse slot {} actor 0x{:x}", slotIndex(slot),
                       reinterpret_cast<uintptr_t>(horse));
    clearSlot(state);

    if (slot == PlayerSlot::Primary) {
        for (int i = 1; i < kPlayerSlotCount; i++) {
            releaseHorseForSlot(static_cast<PlayerSlot>(i));
        }
    }
}

daHorse_c* getHorse(PlayerSlot slot) {
    const HorseSlotState* state = stateForSlotConst(slot);
    if (state != nullptr && state->horse != nullptr) {
        return state->horse;
    }

    return slot == PlayerSlot::Primary ? dComIfGp_getHorseActor() : nullptr;
}

daHorse_c* getHorseForPlayer(const daAlink_c* player) {
    PlayerSlot slot = getSlotForActor(player);
    if (slot == PlayerSlot::Invalid) {
        slot = PlayerSlot::Primary;
    }

    daHorse_c* horse = getHorse(slot);
    return horse != nullptr ? horse : getHorse(PlayerSlot::Primary);
}

daAlink_c* getPlayerForHorse(const daHorse_c* horse) {
    PlayerSlot slot = getSlotForHorse(horse);
    if (slot == PlayerSlot::Invalid) {
        slot = PlayerSlot::Primary;
    }

    fopAc_ac_c* player = getPlayer(slot);
    if (player == nullptr) {
        player = getPrimaryPlayer();
    }
    if (player == nullptr) {
        player = daAlink_getAlinkActorClass();
    }

    return static_cast<daAlink_c*>(player);
}

PlayerSlot getSlotForHorse(const daHorse_c* horse) {
    if (horse == nullptr) {
        return PlayerSlot::Invalid;
    }

    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (s_horses[i].horse == horse) {
            return static_cast<PlayerSlot>(i);
        }
    }

    return horse == dComIfGp_getHorseActor() ? PlayerSlot::Primary : PlayerSlot::Invalid;
}

bool isCanonicalHorse(const daHorse_c* horse) {
    return horse != nullptr && horse == dComIfGp_getHorseActor();
}

bool isAdditionalHorse(const daHorse_c* horse) {
    if (horse == nullptr) {
        return false;
    }

    const PlayerSlot slot = getSlotForHorse(horse);
    return (slot != PlayerSlot::Invalid && slot != PlayerSlot::Primary) ||
           isAdditionalHorseSpawnRequest(horse);
}

bool shouldPresentLashMeter(PlayerSlot slot) {
    daHorse_c* horse = getHorse(slot);
    if (horse == nullptr || horse->checkRodeoMode()) {
        return false;
    }

    daAlink_c* player = getPlayerForHorse(horse);
    return player != nullptr && player->checkHorseRideNotReady();
}

bool anyHorseNeedsLashMeter() {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (shouldPresentLashMeter(static_cast<PlayerSlot>(i))) {
            return true;
        }
    }

    return false;
}

void ensureHorseForSlot(PlayerSlot slot) {
    HorseSlotState* state = stateForSlot(slot);
    daHorse_c* canonical = getHorse(PlayerSlot::Primary);
    if (state == nullptr || slot == PlayerSlot::Primary || getPlayer(slot) == nullptr ||
        canonical == nullptr || state->horse != nullptr ||
        state->pendingSpawnId != fpcM_ERROR_PROCESS_ID_e)
    {
        return;
    }

    cXyz pos = canonical->current.pos;
    const f32 offset = 160.0f * slotIndex(slot);
    pos.x += cM_scos(canonical->shape_angle.y) * offset;
    pos.z -= cM_ssin(canonical->shape_angle.y) * offset;
    csXyz angle = canonical->shape_angle;

    layer_class* savedLayer = fpcLy_CurrentLayer();
    base_process_class* playScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e);
    if (playScene != nullptr) {
        fpcLy_SetCurrentLayer(&((process_node_class*)playScene)->layer);
    }

    const int spawnArgument = kFirstAdditionalHorseSpawnArgument - (slotIndex(slot) - 1);
    state->pendingSpawnId = fopAcM_create(
        fpcNm_HORSE_e,
        fopAcM_GetParam(canonical),
        &pos,
        canonical->current.roomNo,
        &angle,
        nullptr,
        static_cast<s8>(spawnArgument)
    );

    fpcLy_SetCurrentLayer(savedLayer);
    CoopHorseLog.debug("requested horse slot {} spawn id {}", slotIndex(slot),
                       static_cast<unsigned int>(state->pendingSpawnId));
}

void ensureAdditionalHorses() {
    for (int i = 1; i < kPlayerSlotCount; i++) {
        ensureHorseForSlot(static_cast<PlayerSlot>(i));
    }
}

void releaseHorseForSlot(PlayerSlot slot) {
    HorseSlotState* state = stateForSlot(slot);
    if (state == nullptr || slot == PlayerSlot::Primary) {
        return;
    }

    daHorse_c* horse = state->horse;
    const fpc_ProcID pendingSpawnId = state->pendingSpawnId;

    if (horse != nullptr) {
        CoopHorseLog.debug("requested horse slot {} delete actor 0x{:x}", slotIndex(slot),
                           reinterpret_cast<uintptr_t>(horse));
        // Co-op: actor deletion is deferred; unregister clears wrappers after the horse is done.
        fopAcM_delete(horse);
    } else if (pendingSpawnId != fpcM_ERROR_PROCESS_ID_e) {
        clearSlot(state);
        CoopHorseLog.debug("requested horse slot {} pending delete id {}", slotIndex(slot),
                           static_cast<unsigned int>(pendingSpawnId));
        fopAcM_delete(pendingSpawnId);
    }
}

fpc_ProcID getPendingHorseSpawnId(PlayerSlot slot) {
    const HorseSlotState* state = stateForSlotConst(slot);
    return state != nullptr ? state->pendingSpawnId : fpcM_ERROR_PROCESS_ID_e;
}

J3DAnmTransform* localizeAnimationTransform(daHorse_c* horse, J3DAnmTransform* animation) {
    if (horse == nullptr || animation == nullptr || !isAdditionalHorse(horse)) {
        return animation;
    }

    HorseSlotState* state = stateForHorse(horse);
    if (state == nullptr) {
        return animation;
    }

    for (const HorseAnimationClone& clone : state->animations) {
        if (clone.source == animation) {
            return clone.localized;
        }
    }

    // Co-op: archive BCK wrappers share immutable key data but their frame is mutable per horse.
    mDoExt_transAnmBas* localized =
        JKR_NEW mDoExt_transAnmBas(*static_cast<mDoExt_transAnmBas*>(animation));
    if (localized == nullptr) {
        return animation;
    }

    state->animations.push_back({animation, localized});
    return localized;
}

int getLocalizedAnimationCount(const daHorse_c* horse) {
    const HorseSlotState* state = stateForHorse(horse);
    return state != nullptr ? static_cast<int>(state->animations.size()) : 0;
}

HorseReinSimulationState* getReinSimulationState(daHorse_c* horse) {
    HorseSlotState* state = stateForSlot(getSlotForHorse(horse));
    return state != nullptr ? &state->reins : nullptr;
}

const HorseReinSimulationState* getReinSimulationState(const daHorse_c* horse) {
    const HorseSlotState* state = stateForSlotConst(getSlotForHorse(horse));
    return state != nullptr ? &state->reins : nullptr;
}

void copyReinSimulationState(daHorse_c* horse, const cXyz* points, int count) {
    HorseReinSimulationState* state = getReinSimulationState(horse);
    if (state == nullptr) {
        return;
    }

    if (points == nullptr || count <= 0) {
        state->currentValid = false;
        state->currentCount = 0;
        return;
    }

    const int copyCount = std::min(count, kHorseReinSimulationMaxPoints);
    const uint64_t sequence = dusk::frame_interp::sim_tick_seq();
    if (sequence != state->rolledSequence) {
        state->rolledSequence = sequence;
        if (state->currentValid && state->currentCount > 0) {
            std::memcpy(state->previous, state->current, state->currentCount * sizeof(cXyz));
            state->previousCount = state->currentCount;
            state->previousValid = true;
        }
    }

    std::memcpy(state->current, points, copyCount * sizeof(cXyz));
    state->currentCount = copyCount;
    state->currentValid = true;
}

void lerpRegisteredHorseReins(f32 alpha) {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (s_horses[i].horse != nullptr) {
            s_horses[i].horse->lerpControlPoints(alpha);
        }
    }
}

}  // namespace dusk::coop::horse_owner
