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
#include "dusk/coop/message_owner.h"
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
    fpc_ProcID midnaId = fpcM_ERROR_PROCESS_ID_e;
    fpc_ProcID pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
};

struct State {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* partner = nullptr;
    fpc_ProcID partnerId = fpcM_ERROR_PROCESS_ID_e;
    bool presentationActive = false;
    bool awaitingEventStart = false;
    bool pendingEnd = false;
    bool talkCameraSeededDuringStartup = false;
    bool talkCameraStableReseeded = false;
};

struct AbilityState {
    daAlink_c* player = nullptr;
    fpc_ProcID playerId = fpcM_ERROR_PROCESS_ID_e;
    fopAc_ac_c* partner = nullptr;
    fpc_ProcID partnerId = fpcM_ERROR_PROCESS_ID_e;
};

struct TransformBlockSearch {
    daAlink_c* player = nullptr;
    bool far = false;
};

MidnaSlotState s_midnas[kPlayerSlotCount];
State s_state;
AbilityState s_abilities[kPlayerSlotCount];

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

fopAc_ac_c* liveServicePartner() {
    if (s_state.partner == nullptr || s_state.partnerId == fpcM_ERROR_PROCESS_ID_e) {
        return nullptr;
    }

    return fpcM_SearchByID(s_state.partnerId) == static_cast<base_process_class*>(s_state.partner)
               ? s_state.partner
               : nullptr;
}

AbilityState* abilityStateForPlayer(const daAlink_c* player) {
    if (player == nullptr) {
        return nullptr;
    }

    const PlayerSlot slot = getSlotForActor(static_cast<const fopAc_ac_c*>(player));
    return isValidSlot(slot) ? &s_abilities[slotIndex(slot)] : nullptr;
}

fopAc_ac_c* liveAbilityPartner(const daAlink_c* player) {
    AbilityState* state = abilityStateForPlayer(player);
    if (state == nullptr || state->player != player ||
        state->playerId != fopAcM_GetID(player) || state->partner == nullptr ||
        state->partnerId == fpcM_ERROR_PROCESS_ID_e ||
        fpcM_SearchByID(state->partnerId) != static_cast<base_process_class*>(state->partner))
    {
        if (state != nullptr) {
            *state = AbilityState{};
        }
        return nullptr;
    }

    return state->partner;
}

MidnaSlotState* stateForSlot(PlayerSlot slot) {
    if (!isValidSlot(slot)) {
        return nullptr;
    }

    return &s_midnas[slotIndex(slot)];
}

daMidna_c* liveMidna(MidnaSlotState* state) {
    if (state == nullptr || state->midna == nullptr ||
        state->midnaId == fpcM_ERROR_PROCESS_ID_e)
    {
        return nullptr;
    }

    if (fpcM_SearchByID(state->midnaId) ==
        static_cast<base_process_class*>(static_cast<fopAc_ac_c*>(state->midna)))
    {
        return state->midna;
    }

    state->midna = nullptr;
    state->midnaId = fpcM_ERROR_PROCESS_ID_e;
    return nullptr;
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

void beginServiceInternal(daAlink_c* player, fopAc_ac_c* partner) {
    if (s_state.slot != PlayerSlot::Invalid) {
        endService();
    }

    s_state = State{};
    PlayerSlot slot = normalizeSlot(getSlotForActor(player));
    s_state.slot = slot;
    s_state.partner = partner != nullptr ? partner : static_cast<fopAc_ac_c*>(getMidna(slot));
    s_state.partnerId = s_state.partner != nullptr ? fopAcM_GetID(s_state.partner)
                                                   : fpcM_ERROR_PROCESS_ID_e;
    s_state.awaitingEventStart = true;
    s_state.pendingEnd = false;
    s_state.talkCameraSeededDuringStartup = false;
    s_state.talkCameraStableReseeded = false;

    beginPresentation(slot);
    // Co-op: interactive Midna service owns the first fullscreen dialogue handoff.
    message_owner::begin(slot, static_cast<fopAc_ac_c*>(player),
                         static_cast<fopAc_ac_c*>(getMidna(slot)), true,
                         message_owner::BeginSource::MidnaService);
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
    state->midnaId = fopAcM_GetID(midna);
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
    state->midnaId = fpcM_ERROR_PROCESS_ID_e;
    state->pendingSpawnId = fpcM_ERROR_PROCESS_ID_e;
    s_abilities[slotIndex(slot)] = AbilityState{};
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
    if (daMidna_c* midna = liveMidna(state)) {
        return midna;
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
    beginServiceInternal(player, partner);
}

void beginAbilityService(daAlink_c* player, fopAc_ac_c* partner) {
    AbilityState* state = abilityStateForPlayer(player);
    if (state == nullptr || partner == nullptr) {
        return;
    }

    // Co-op: non-dialogue Midna abilities are concurrent slot-local services,
    // independent of the one singular Midna conversation owner.
    state->player = player;
    state->playerId = fopAcM_GetID(player);
    state->partner = partner;
    state->partnerId = fopAcM_GetID(partner);
}

void endAbilityService(const daAlink_c* player, const fopAc_ac_c* partner) {
    AbilityState* state = abilityStateForPlayer(player);
    if (state != nullptr && liveAbilityPartner(player) == partner) {
        *state = AbilityState{};
    }
}

void endAbilityServicesForPartner(const fopAc_ac_c* partner) {
    if (partner == nullptr) {
        return;
    }

    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (s_abilities[i].partner == partner) {
            s_abilities[i] = AbilityState{};
        }
    }
}

void requestEndService() {
    if (s_state.slot == PlayerSlot::Invalid) {
        return;
    }

    // Co-op: native Midna/message teardown can happen during actor execution,
    // while the event camera still consumes the talk state later in the same
    // management pass. Keep the retained owner actors alive until post-camera.
    s_state.pendingEnd = true;
}

void endService() {
    const PlayerSlot service_slot = s_state.slot;
    if (service_slot != PlayerSlot::Invalid && message_owner::isActive() &&
        message_owner::currentSlot() == normalizeSlot(service_slot))
    {
        // Co-op: Midna service owns the full transform prompt lifetime. Once
        // native Midna accepts, cancels, or exits, release any refreshed message
        // owner for the same slot before native actor pointers can be torn down.
        message_owner::end();
    }

    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::MidnaService);
    }
    s_state = State{};
}

void finishPendingEndService() {
    if (!s_state.pendingEnd) {
        return;
    }

    if (dComIfGp_evmng_cameraPlay()) {
        // Co-op: cancelled Midna messages can leave the native TALK camera
        // running after the message object closes; keep retained actors until
        // the camera manager stops consuming talk state.
        return;
    }

    endService();
}

void updateService() {
    if (s_state.slot == PlayerSlot::Invalid) {
        return;
    }

    daAlink_c* player = currentPlayer();
    dEvt_control_c* event = dComIfGp_getEvent();
    const bool event_active = dComIfGp_event_runCheck() || dMsgObject_isTalkNowCheck() ||
                              (player != nullptr && player->checkPlayerDemoMode());
    if (s_state.awaitingEventStart) {
        if (event_active) {
            s_state.awaitingEventStart = false;
        } else if (event != nullptr && event->mNum > 0) {
            // Co-op: native talk events are ordered before they run; keep the
            // provisional Midna presentation alive through that queued state.
            return;
        }
    }

    if (!dComIfGp_event_runCheck() && !dMsgObject_isTalkNowCheck() &&
        (player == nullptr || !player->checkPlayerDemoMode()))
    {
        requestEndService();
    }
}

void reset() {
    s_state = State{};
    for (int i = 0; i < kPlayerSlotCount; i++) {
        s_abilities[i] = AbilityState{};
    }
}

bool isServiceActive() {
    return s_state.slot != PlayerSlot::Invalid;
}

bool retainsTalkCamera() {
    return isServiceActive() && currentPlayer() != nullptr && getMidna(currentSlot()) != nullptr;
}

void markTalkCameraSeed(daMidna_c* midna, bool startupUnstable) {
    if (!retainsTalkCamera() || midna == nullptr || midna != getMidna(currentSlot())) {
        return;
    }

    s_state.talkCameraSeededDuringStartup = startupUnstable;
    if (!startupUnstable) {
        s_state.talkCameraStableReseeded = true;
    }
}

bool shouldReseedTalkCameraForStableMidna(daMidna_c* midna, bool poseReady) {
    if (!retainsTalkCamera() || midna == nullptr || midna != getMidna(currentSlot())) {
        return false;
    }

    // Co-op: manual Midna dialogue is presented as soon as the service is
    // accepted, but the native talk camera can seed while Midna is still in her
    // shadow-appear wait pose. Once the retained Midna reaches a usable talk
    // pose, reseed the camera once instead of keeping the startup aim forever.
    return s_state.talkCameraSeededDuringStartup && !s_state.talkCameraStableReseeded && poseReady;
}

void markTalkCameraStableReseeded() {
    if (!retainsTalkCamera()) {
        return;
    }

    s_state.talkCameraStableReseeded = true;
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

fopAc_ac_c* currentPartner() {
    return isServiceActive() ? liveServicePartner() : nullptr;
}

daAlink_c* messageFlowPlayer() {
    daAlink_c* player = isServiceActive() ? currentPlayer() : eventOwnerPlayer();
    return player != nullptr ? player : eventOwnerPlayer();
}

fopAc_ac_c* talkPartnerForPlayer(const daAlink_c* player) {
    if (player == nullptr) {
        return nullptr;
    }

    if (isServiceActive() && shouldConsumeAlinkStaff(player)) {
        if (daMidna_c* midna = getMidnaForPlayer(player)) {
            return static_cast<fopAc_ac_c*>(midna);
        }

        return currentPartner();
    }

    if (message_owner::isActive() && message_owner::currentPlayer() == player) {
        if (fopAc_ac_c* speaker = message_owner::speaker()) {
            return speaker;
        }
    }

    return fopAcM_getTalkEventPartner(const_cast<daAlink_c*>(player));
}

fopAc_ac_c* abilityPartnerForPlayer(const daAlink_c* player) {
    return liveAbilityPartner(player);
}

bool isServicePartner(const fopAc_ac_c* partner) {
    if (partner == nullptr) {
        return false;
    }

    const s16 name = fpcM_GetName(partner);
    return name == fpcNm_MIDNA_e || name == fpcNm_Tag_Mhint_e ||
           name == fpcNm_Tag_Mstop_e || name == fpcNm_Tag_Mmsg_e ||
           name == fpcNm_Tag_Wljump_e;
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
    if (isServiceActive() && isServicePartner(currentPartner())) {
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
        partner = currentPartner();
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
