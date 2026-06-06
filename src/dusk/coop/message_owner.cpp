#include "dusk/coop/message_owner.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/event_owner.h"
#include "dusk/coop/event_presentation.h"
#include "dusk/coop/midna_owner.h"

namespace dusk::coop::message_owner {
namespace {

struct State {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* listener = nullptr;
    fopAc_ac_c* speaker = nullptr;
    bool presentationActive = false;
    DebugState debug;
};

State s_state;

PlayerSlot normalizeSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Invalid ? PlayerSlot::Primary : slot;
}

daAlink_c* playerForSlot(PlayerSlot slot) {
    fopAc_ac_c* player = getPlayer(normalizeSlot(slot));
    if (player == nullptr) {
        player = dComIfGp_getPlayer(0);
    }

    return static_cast<daAlink_c*>(player);
}

void refreshDebug(Transition transition) {
    s_state.debug.active = s_state.slot != PlayerSlot::Invalid;
    s_state.debug.presentationActive = s_state.presentationActive;
    s_state.debug.lastTransition = transition;
    s_state.debug.slot = currentSlot();
    s_state.debug.pad = currentPad();
    s_state.debug.listener = reinterpret_cast<uintptr_t>(s_state.listener);
    s_state.debug.speaker = reinterpret_cast<uintptr_t>(s_state.speaker);
    s_state.debug.revision++;
}

}  // namespace

void begin(PlayerSlot slot, fopAc_ac_c* listenerActor, fopAc_ac_c* speakerActor, bool fullscreen) {
    if (midna_owner::isServiceActive()) {
        slot = midna_owner::currentSlot();
        listenerActor = static_cast<fopAc_ac_c*>(midna_owner::currentPlayer());
        speakerActor = static_cast<fopAc_ac_c*>(midna_owner::getMidna(slot));
    } else if (slot == PlayerSlot::Invalid) {
        slot = event_owner::ownerSlotForActor(listenerActor);
    }

    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::Dialogue);
    }

    s_state.slot = normalizeSlot(slot);
    s_state.listener = listenerActor != nullptr ? listenerActor :
                       static_cast<fopAc_ac_c*>(playerForSlot(s_state.slot));
    s_state.speaker = speakerActor;
    s_state.presentationActive = false;

    if (fullscreen) {
        event_presentation::Options options;
        options.fullscreenSlot = s_state.slot;
        options.hideNonPresenterVisuals = true;
        event_presentation::begin(event_presentation::Source::Dialogue, options);
        s_state.presentationActive = true;
    }

    refreshDebug(Transition::Begin);
}

void end() {
    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::Dialogue);
    }

    s_state.slot = PlayerSlot::Invalid;
    s_state.listener = nullptr;
    s_state.speaker = nullptr;
    s_state.presentationActive = false;
    refreshDebug(Transition::End);
}

void reset() {
    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::Dialogue);
    }

    s_state = State{};
    refreshDebug(Transition::Reset);
}

bool isActive() {
    return s_state.slot != PlayerSlot::Invalid;
}

PlayerSlot currentSlot() {
    return isActive() ? normalizeSlot(s_state.slot) : event_owner::currentOwnerSlot();
}

int currentPad() {
    return getPadForSlot(currentSlot());
}

daAlink_c* currentPlayer() {
    return playerForSlot(currentSlot());
}

fopAc_ac_c* listener() {
    return s_state.listener != nullptr ? s_state.listener :
           static_cast<fopAc_ac_c*>(currentPlayer());
}

fopAc_ac_c* speaker() {
    return s_state.speaker;
}

bool isPresenterSlot(PlayerSlot slot) {
    return isActive() && normalizeSlot(slot) == currentSlot();
}

const DebugState& getDebugState() {
    return s_state.debug;
}

const char* transitionName(Transition transition) {
    switch (transition) {
    case Transition::Begin:
        return "begin";
    case Transition::End:
        return "end";
    case Transition::Reset:
        return "reset";
    default:
        return "none";
    }
}

}  // namespace dusk::coop::message_owner
