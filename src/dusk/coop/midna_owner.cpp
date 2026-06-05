#include "dusk/coop/midna_owner.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#include "d/actor/d_a_tag_mhint.h"
#include "d/actor/d_a_tag_mmsg.h"
#include "d/actor/d_a_tag_mstop.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_kankyo.h"
#include "d/d_msg_object.h"
#include "dusk/settings.h"
#include "dusk/coop/event_owner.h"
#include "dusk/coop/event_presentation.h"
#include "dusk/coop/player_camera_status.h"
#include "dusk/logging.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstring>

namespace dusk::coop::midna_owner {
namespace {

aurora::Module CoopMidnaLog("dusk::coop::midna_owner");
constexpr u32 kTalkStatusFlag = 0x10;

struct MidnaSlotState {
    daMidna_c* midna = nullptr;
    fpc_ProcID pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
};

struct State {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* partner = nullptr;
    bool presentationActive = false;
};

struct TransformBlockSearch {
    daAlink_c* player = nullptr;
    bool far = false;
};

MidnaSlotState s_midnas[kPlayerSlotCount];
State s_state;

constexpr bool isValidSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Slot0 || slot == PlayerSlot::Slot1 ||
           slot == PlayerSlot::Slot2 || slot == PlayerSlot::Slot3;
}

constexpr int slotIndex(PlayerSlot slot) {
    return static_cast<int>(slot);
}

PlayerSlot normalizeSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Invalid ? PlayerSlot::Primary : slot;
}

MidnaSlotState* stateForSlot(PlayerSlot slot) {
    if (!isValidSlot(slot)) {
        return nullptr;
    }

    return &s_midnas[slotIndex(slot)];
}

daAlink_c* playerForSlot(PlayerSlot slot) {
    fopAc_ac_c* player = getPlayer(normalizeSlot(slot));
    if (player == nullptr) {
        player = dComIfGp_getPlayer(0);
    }
    return static_cast<daAlink_c*>(player);
}

daAlink_c* eventOwnerPlayer() {
    daAlink_c* player = static_cast<daAlink_c*>(event_owner::ownerPlayerForActor(nullptr));
    return player != nullptr ? player : playerForSlot(PlayerSlot::Primary);
}

void* searchNpcForTransform(void* actorVoid, void* data) {
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(actorVoid);
    TransformBlockSearch* search = static_cast<TransformBlockSearch*>(data);
    daAlink_c* player = search != nullptr ? search->player : nullptr;
    if (player == nullptr) {
        return nullptr;
    }

    cXyz player_pos = player->current.pos;
    player_pos.y += 100.0f;

    if (fopAcM_GetGroup(actor) == fopAc_NPC_e && !fopAcM_CheckStatus(actor, 0x8000000) &&
        std::fabs(player_pos.y - actor->eyePos.y) <= 700.0f)
    {
        const f32 dist = actor->eyePos.absXZ(player_pos);
        if (dist <= player->getMetamorphoseNearDis()) {
            search->far = false;
        } else if (dist <= player->getMetamorphoseFarDis() &&
                   fopAcM_seenPlayerAngleY(actor) <= player->getMetamorphoseFarAngle())
        {
            search->far = true;
        } else {
            return nullptr;
        }

        if (!fopAcM_lc_c::lineCheck(&actor->eyePos, &player_pos, actor)) {
            return actor;
        }
    }

    return nullptr;
}

void beginPresentation(PlayerSlot slot) {
    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::MidnaService);
        s_state.presentationActive = false;
    }

    event_presentation::Options options;
    options.fullscreenSlot = normalizeSlot(slot);
    options.hideNonPresenterVisuals = true;
    event_presentation::begin(event_presentation::Source::MidnaService, options);
    s_state.presentationActive = true;
}

}  // namespace

bool isAdditionalMidnaSpawnRequest(const fopAc_ac_c* actor) {
    return getAdditionalMidnaSpawnRequestSlot(actor) != PlayerSlot::Invalid;
}

PlayerSlot getAdditionalMidnaSpawnRequestSlot(const fopAc_ac_c* actor) {
    if (actor == nullptr || actor->argument > kFirstAdditionalMidnaSpawnArgument) {
        return PlayerSlot::Invalid;
    }

    const int slot = 1 + (kFirstAdditionalMidnaSpawnArgument - actor->argument);
    if (slot < 1 || slot >= kPlayerSlotCount) {
        return PlayerSlot::Invalid;
    }

    return static_cast<PlayerSlot>(slot);
}

void registerMidna(PlayerSlot slot, daMidna_c* midna) {
    MidnaSlotState* state = stateForSlot(slot);
    if (state == nullptr || midna == nullptr) {
        return;
    }

    state->midna = midna;
    state->pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
    CoopMidnaLog.debug("registered Midna slot {} actor 0x{:x}", slotIndex(slot),
                       reinterpret_cast<uintptr_t>(midna));
}

void unregisterMidna(PlayerSlot slot, const daMidna_c* midna) {
    MidnaSlotState* state = stateForSlot(slot);
    if (state == nullptr || state->midna != midna) {
        return;
    }

    state->midna = nullptr;
    state->pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
    CoopMidnaLog.debug("unregistered Midna slot {} actor 0x{:x}", slotIndex(slot),
                       reinterpret_cast<uintptr_t>(midna));

    if (slot == PlayerSlot::Primary) {
        for (int i = 1; i < kPlayerSlotCount; i++) {
            releaseMidnaForSlot(static_cast<PlayerSlot>(i));
        }
    }
}

daMidna_c* getMidna(PlayerSlot slot) {
    MidnaSlotState* state = stateForSlot(slot);
    if (state != nullptr && state->midna != nullptr) {
        return state->midna;
    }

    return slot == PlayerSlot::Primary ? daPy_py_c::getMidnaActor() : nullptr;
}

daMidna_c* getMidnaForPlayer(const daAlink_c* player) {
    PlayerSlot slot = getSlotForActor(static_cast<const fopAc_ac_c*>(player));
    if (slot == PlayerSlot::Invalid) {
        slot = PlayerSlot::Primary;
    }

    daMidna_c* midna = getMidna(slot);
    if (slot != PlayerSlot::Primary) {
        return midna;
    }

    return midna != nullptr ? midna : getMidna(PlayerSlot::Primary);
}

daAlink_c* getPlayerForMidna(const daMidna_c* midna) {
    PlayerSlot slot = getSlotForMidna(midna);
    return playerForSlot(slot);
}

PlayerSlot getSlotForMidna(const daMidna_c* midna) {
    if (midna == nullptr) {
        return PlayerSlot::Invalid;
    }

    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (s_midnas[i].midna == midna) {
            return static_cast<PlayerSlot>(i);
        }
    }

    if (isAdditionalMidnaSpawnRequest(static_cast<const fopAc_ac_c*>(midna))) {
        return getAdditionalMidnaSpawnRequestSlot(static_cast<const fopAc_ac_c*>(midna));
    }

    return midna == daPy_py_c::getMidnaActor() ? PlayerSlot::Primary : PlayerSlot::Invalid;
}

void ensureMidnaForSlot(PlayerSlot slot) {
    MidnaSlotState* state = stateForSlot(slot);
    daMidna_c* canonical = getMidna(PlayerSlot::Primary);
    fopAc_ac_c* player = getPlayer(slot);
    if (state == nullptr || slot == PlayerSlot::Primary || player == nullptr ||
        canonical == nullptr || state->midna != nullptr ||
        state->pendingSpawnId != fpcM_ERROR_PROCESS_ID_e)
    {
        return;
    }

    const int spawnArgument = kFirstAdditionalMidnaSpawnArgument - (slotIndex(slot) - 1);
    layer_class* savedLayer = fpcLy_CurrentLayer();
    base_process_class* playScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e);
    if (playScene != nullptr) {
        fpcLy_SetCurrentLayer(&((process_node_class*)playScene)->layer);
    }

    state->pendingSpawnId = fopAcM_create(
        fpcNm_MIDNA_e,
        fopAcM_GetParam(canonical),
        &player->current.pos,
        player->current.roomNo,
        &player->shape_angle,
        nullptr,
        static_cast<s8>(spawnArgument)
    );

    fpcLy_SetCurrentLayer(savedLayer);
    CoopMidnaLog.debug("requested Midna slot {} spawn id {}", slotIndex(slot),
                       static_cast<unsigned int>(state->pendingSpawnId));
}

void ensureAdditionalMidnas() {
    for (int i = 1; i < kPlayerSlotCount; i++) {
        ensureMidnaForSlot(static_cast<PlayerSlot>(i));
    }
}

void releaseMidnaForSlot(PlayerSlot slot) {
    MidnaSlotState* state = stateForSlot(slot);
    if (state == nullptr || slot == PlayerSlot::Primary) {
        return;
    }

    daMidna_c* midna = state->midna;
    const fpc_ProcID pendingSpawnId = state->pendingSpawnId;

    if (state->midna != nullptr) {
        CoopMidnaLog.debug("requested Midna slot {} delete actor 0x{:x}", slotIndex(slot),
                           reinterpret_cast<uintptr_t>(midna));
        fopAcM_delete(static_cast<fopAc_ac_c*>(midna));
    } else if (pendingSpawnId != fpcM_ERROR_PROCESS_ID_e) {
        state->pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
        CoopMidnaLog.debug("requested Midna slot {} pending delete id {}", slotIndex(slot),
                           static_cast<unsigned int>(pendingSpawnId));
        fopAcM_delete(pendingSpawnId);
    }
}

bool canUseService(const daAlink_c* player) {
    if (player == nullptr) {
        return false;
    }

    if (!isAdditionalPlayer(static_cast<const fopAc_ac_c*>(player))) {
        return player->checkMidnaRide();
    }

    ensureMidnaForSlot(getSlotForActor(static_cast<const fopAc_ac_c*>(player)));
    daMidna_c* midna = getMidnaForPlayer(player);
    if (midna == nullptr) {
        return false;
    }

    // Co-op: additional slots need their own Midna service actor before they can
    // own the manual transform conversation.
    return daPy_py_c::checkFirstMidnaDemo() && !midna->checkWolfNoPos();
}

void beginService(daAlink_c* player, fopAc_ac_c* partner) {
    PlayerSlot slot = normalizeSlot(getSlotForActor(player));
    s_state.slot = slot;
    s_state.partner = partner != nullptr ? partner : static_cast<fopAc_ac_c*>(getMidna(slot));
    beginPresentation(slot);
}

void endService() {
    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::MidnaService);
    }
    s_state = State{};
}

void updateService() {
    if (s_state.slot == PlayerSlot::Invalid) {
        return;
    }

    daAlink_c* player = currentPlayer();
    if (!dComIfGp_event_runCheck() && !dMsgObject_isTalkNowCheck() &&
        (player == nullptr || !player->checkPlayerDemoMode()))
    {
        endService();
    }
}

void reset() {
    s_state = State{};
}

bool isServiceActive() {
    return s_state.slot != PlayerSlot::Invalid;
}

PlayerSlot currentSlot() {
    if (isServiceActive()) {
        return normalizeSlot(s_state.slot);
    }

    return PlayerSlot::Primary;
}

daAlink_c* currentPlayer() {
    return playerForSlot(currentSlot());
}

daAlink_c* messageFlowPlayer() {
    daAlink_c* player = isServiceActive() ? currentPlayer() : eventOwnerPlayer();
    return player != nullptr ? player : eventOwnerPlayer();
}

bool isServicePartner(const fopAc_ac_c* partner) {
    if (partner == nullptr) {
        return false;
    }

    const s16 name = fpcM_GetName(partner);
    return name == fpcNm_MIDNA_e || name == fpcNm_Tag_Mhint_e ||
           name == fpcNm_Tag_Mstop_e || name == fpcNm_Tag_Mmsg_e;
}

bool shouldConsumeAlinkStaff(const daAlink_c* player) {
    if (player == nullptr || s_state.slot == PlayerSlot::Invalid) {
        return false;
    }

    return getSlotForActor(static_cast<const fopAc_ac_c*>(player)) == normalizeSlot(s_state.slot);
}

bool shouldSkipAlinkStaff(const daAlink_c* player) {
    if (player == nullptr || shouldConsumeAlinkStaff(player)) {
        return false;
    }

    // Co-op: Midna's transform handoff retargets Pt2 to the retained ALINK, so
    // the service partner is the reliable signal for skipping non-owner staff.
    if (isServiceActive() && isServicePartner(s_state.partner)) {
        return true;
    }

    if (event_owner::isCurrentOwner(static_cast<const fopAc_ac_c*>(player))) {
        return false;
    }

    fopAc_ac_c* partner = dComIfGp_event_getPt2();
    // Co-op: generic Alink staff tracks are singular; non-owners skip Midna tracks
    // selected by the requester instead of reading their own empty talk partner.
    if (partner == nullptr) {
        partner = fopAcM_getTalkEventPartner(eventOwnerPlayer());
    }
    if (partner == nullptr) {
        partner = s_state.partner;
    }
    return isServicePartner(partner);
}

s32 orderPotentialEvent(fopAc_ac_c* requester, u16 flags, u16 hindFlags, u16 priority) {
    if (requester == nullptr) {
        return 0;
    }

    if (!dComIfGp_getEvent()->isOrderOK() &&
        (!(flags & 0x400) || !dComIfGp_getEvent()->isChangeOK(requester)))
    {
        return 0;
    }

    if (priority == 0) {
        priority = 0xFF;
    }

    // Co-op: each slot's Midna service actor requests its own potential event,
    // while Pt2 remains the retained ALINK that accepted the transform.
    return dComIfGp_event_order(dEvt_type_POTENTIAL_e, priority, flags, hindFlags, requester,
                                currentPlayer(), -1, -1);
}

bool currentPlayerIsWolf() {
    daAlink_c* player = messageFlowPlayer();
    return player != nullptr && player->checkWolf();
}

bool currentPlayerRidesHorseOrBoar() {
    daAlink_c* player = messageFlowPlayer();
    return player != nullptr && (player->checkHorseRide() || player->checkBoarRide());
}

int transformBlockReasonForPlayer(const daAlink_c* player) {
    if (player == nullptr) {
        return 0;
    }

    if (std::strcmp("F_SP116", dComIfGp_getStartStageName()) == 0 &&
        dComIfGs_isSaveDunSwitch(60))
    {
        return 4;
    }

    if (g_env_light.mEvilInitialized & 0x80) {
        return 3;
    }

    TransformBlockSearch search;
    search.player = const_cast<daAlink_c*>(player);
    if (!dusk::getSettings().game.canTransformAnywhere &&
        fopAcIt_Judge(searchNpcForTransform, &search))
    {
        return search.far ? 2 : 1;
    }

    return 0;
}

bool canTransformNow(const daAlink_c* player) {
    if (!canUseService(player)) {
        return false;
    }

    if (!dComIfGs_isEventBit(0xD04)) {
        return false;
    }

    return transformBlockReasonForPlayer(player) == 0;
}

int currentTransformBlockReason() {
    return transformBlockReasonForPlayer(messageFlowPlayer());
}

u16 currentMidnaMsgNum() {
    daAlink_c* player = messageFlowPlayer();
    return player != nullptr ? player->getMidnaMsgNum() : 0xffff;
}

void markCurrentMidnaMsgUsed() {
    daAlink_c* player = messageFlowPlayer();
    if (player != nullptr) {
        player->setMidnaMsg();
    }
}

bool checkTalkStatus(const daAlink_c* player) {
    if (isAdditionalPlayer(static_cast<const fopAc_ac_c*>(player))) {
        return player_camera_status::checkStatus0ForPlayer(player, kTalkStatusFlag) != 0;
    }

    return dComIfGp_checkPlayerStatus0(0, kTalkStatusFlag) != 0;
}

void setTalkStatus(daAlink_c* player) {
    if (isAdditionalPlayer(static_cast<const fopAc_ac_c*>(player))) {
        player_camera_status::setStatus0ForPlayer(player, kTalkStatusFlag);
    } else {
        dComIfGp_setPlayerStatus0(0, kTalkStatusFlag);
    }
}

void clearTalkStatus(daAlink_c* player) {
    if (isAdditionalPlayer(static_cast<const fopAc_ac_c*>(player))) {
        player_camera_status::clearStatus0ForPlayer(player, kTalkStatusFlag);
    } else {
        dComIfGp_clearPlayerStatus0(0, kTalkStatusFlag);
    }
}

}  // namespace dusk::coop::midna_owner
