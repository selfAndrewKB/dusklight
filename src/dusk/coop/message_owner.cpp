#include "dusk/coop/message_owner.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/event_owner.h"
#include "dusk/coop/event_presentation.h"
#include "dusk/coop/midna_owner.h"
#include "dusk/diagnostics.h"

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
    if (transition == Transition::Reset) {
        s_state.debug.lastBeginSource = BeginSource::None;
    }
    s_state.debug.slot = currentSlot();
    s_state.debug.pad = currentPad();
    s_state.debug.presenter = reinterpret_cast<uintptr_t>(presenterActor());
    s_state.debug.listener = reinterpret_cast<uintptr_t>(s_state.listener);
    s_state.debug.speaker = reinterpret_cast<uintptr_t>(s_state.speaker);
    s_state.debug.revision++;
}

}  // namespace

void begin(PlayerSlot slot, fopAc_ac_c* listenerActor, fopAc_ac_c* speakerActor, bool fullscreen,
           BeginSource source) {
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
    s_state.debug.lastBeginSource = source;
    s_state.debug.fallbackActor = 0;
    s_state.debug.talkCut = -1;

    if (fullscreen) {
        event_presentation::Options options;
        options.fullscreenSlot = s_state.slot;
        options.hideNonPresenterVisuals = true;
        event_presentation::begin(event_presentation::Source::Dialogue, options);
        s_state.presentationActive = true;
    }

    refreshDebug(Transition::Begin);
    diagnostics::recordMessageOwnerCheckpoint(
        "begin", static_cast<int>(currentSlot()), currentPad(), presenterActor(), listener(),
        speaker(), fullscreen, s_state.presentationActive, event_presentation::isFullscreen(),
        event_presentation::presenterWindowIndex(), beginSourceName(source));
}

void end() {
    if (s_state.presentationActive) {
        event_presentation::end(event_presentation::Source::Dialogue);
    }

    diagnostics::recordMessageOwnerCheckpoint(
        "end", static_cast<int>(currentSlot()), currentPad(), presenterActor(), listener(),
        speaker(), false, s_state.presentationActive, event_presentation::isFullscreen(),
        event_presentation::presenterWindowIndex(), beginSourceName(s_state.debug.lastBeginSource));

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

    diagnostics::recordMessageOwnerCheckpoint(
        "reset", static_cast<int>(currentSlot()), currentPad(), presenterActor(), listener(),
        speaker(), false, s_state.presentationActive, event_presentation::isFullscreen(),
        event_presentation::presenterWindowIndex(), beginSourceName(s_state.debug.lastBeginSource));

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

fopAc_ac_c* presenterActor() {
    return static_cast<fopAc_ac_c*>(currentPlayer());
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

void recordTalkCameraDebug(fopAc_ac_c* fallbackActor, int talkCut) {
    s_state.debug.fallbackActor = reinterpret_cast<uintptr_t>(fallbackActor);
    s_state.debug.talkCut = talkCut;
    s_state.debug.presenter = reinterpret_cast<uintptr_t>(presenterActor());
    s_state.debug.listener = reinterpret_cast<uintptr_t>(listener());
    s_state.debug.speaker = reinterpret_cast<uintptr_t>(speaker());
    s_state.debug.revision++;
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

const char* beginSourceName(BeginSource source) {
    switch (source) {
    case BeginSource::MessageAccept:
        return "message_accept";
    case BeginSource::MessageAcceptDemo:
        return "message_accept_demo";
    case BeginSource::TalkStartFallback:
        return "talk_start_fallback";
    case BeginSource::MidnaLocalString:
        return "midna_local_string";
    case BeginSource::MidnaService:
        return "midna_service";
    default:
        return "none";
    }
}

}  // namespace dusk::coop::message_owner
