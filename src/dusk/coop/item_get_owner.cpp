#include "dusk/coop/item_get_owner.h"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/event_presentation.h"
#include "f_op/f_op_actor_mng.h"

#include <cstring>

namespace dusk::coop::item_get_owner {
namespace {

struct PendingOwner {
    fopAc_ac_c* source = nullptr;
    fpc_ProcID sourceId = fpcM_ERROR_PROCESS_ID_e;
    fopAc_ac_c* player = nullptr;
    PlayerSlot slot = PlayerSlot::Invalid;
};

struct State {
    PendingOwner pending;
    PlayerSlot slot = PlayerSlot::Invalid;
    bool presentationActive = false;
    bool pendingEnd = false;
};

State s_state;

PlayerSlot normalizeSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Invalid ? PlayerSlot::Primary : slot;
}

bool pendingMatchesCurrentEvent() {
    fopAc_ac_c* source = dComIfGp_event_getPt1();
    return source != nullptr && source == s_state.pending.source &&
           fopAcM_GetID(source) == s_state.pending.sourceId &&
           s_state.pending.slot != PlayerSlot::Invalid &&
           getPlayer(s_state.pending.slot) == s_state.pending.player;
}

PlayerSlot resolveCurrentSlot(const fopAc_ac_c* fallbackPlayer) {
    if (s_state.slot != PlayerSlot::Invalid) {
        return normalizeSlot(s_state.slot);
    }
    if (pendingMatchesCurrentEvent()) {
        return s_state.pending.slot;
    }

    PlayerSlot slot = getSlotForActor(dComIfGp_event_getPt1());
    if (slot == PlayerSlot::Invalid) {
        slot = getSlotForActor(fallbackPlayer);
    }
    return normalizeSlot(slot);
}

void clearPending() {
    s_state.pending = PendingOwner{};
}

}  // namespace

void retainForEventSource(fopAc_ac_c* source, fopAc_ac_c* player) {
    const PlayerSlot slot = getSlotForActor(player);
    if (source == nullptr || player == nullptr || slot == PlayerSlot::Invalid) {
        return;
    }

    s_state.pending.source = source;
    s_state.pending.sourceId = fopAcM_GetID(source);
    s_state.pending.player = player;
    s_state.pending.slot = slot;
}

void begin(daAlink_c* player) {
    if (!isDefaultGetItemEvent()) {
        return;
    }

    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::ItemGet);
    }

    s_state.slot = resolveCurrentSlot(static_cast<fopAc_ac_c*>(player));
    clearPending();
    s_state.pendingEnd = false;

    event_presentation::Options options;
    options.fullscreenSlot = s_state.slot;
    options.hideNonPresenterVisuals = true;
    event_presentation::begin(event_presentation::Source::ItemGet, options);
    s_state.presentationActive = true;
}

void requestEnd() {
    if (isActive()) {
        s_state.pendingEnd = true;
    }
}

void finishPendingEnd() {
    if (s_state.pendingEnd && !dComIfGp_evmng_cameraPlay()) {
        end();
    }
}

void end() {
    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::ItemGet);
    }

    s_state.slot = PlayerSlot::Invalid;
    s_state.presentationActive = false;
    s_state.pendingEnd = false;
    clearPending();
}

void reset() {
    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::ItemGet);
    }
    s_state = State{};
}

bool isActive() {
    return s_state.slot != PlayerSlot::Invalid;
}

bool isDefaultGetItemEvent() {
    return isDefaultGetItemEvent(dComIfGp_getEventManager().getRunEventName());
}

bool isDefaultGetItemEvent(const char* eventName) {
    return eventName != nullptr && std::strcmp(eventName, "DEFAULT_GETITEM") == 0;
}

bool shouldSkipAlinkStaff(const daAlink_c* player) {
    if (!isDefaultGetItemEvent()) {
        return false;
    }

    const PlayerSlot playerSlot =
        getSlotForActor(static_cast<const fopAc_ac_c*>(player));
    return playerSlot == PlayerSlot::Invalid || playerSlot != resolveCurrentSlot(nullptr);
}

PlayerSlot currentSlot() {
    return isActive() ? normalizeSlot(s_state.slot) : resolveCurrentSlot(nullptr);
}

daAlink_c* currentPlayer() {
    fopAc_ac_c* player = getPlayer(currentSlot());
    if (player == nullptr) {
        player = dComIfGp_getPlayer(0);
    }
    return static_cast<daAlink_c*>(player);
}

}  // namespace dusk::coop::item_get_owner
