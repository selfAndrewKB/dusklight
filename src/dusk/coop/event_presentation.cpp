#include "dusk/coop/event_presentation.h"

#include "dusk/coop/camera.h"

namespace dusk::coop::event_presentation {
namespace {

constexpr int kSourceCount = static_cast<int>(Source::Count);
constexpr int kMaxActiveEntries = 32;

struct ActiveEntry {
    Source source = Source::WolfHowl;
    Options options;
};

struct SidecarState {
    ActiveEntry entries[kMaxActiveEntries];
    int entryCount = 0;
    int depths[kSourceCount] = {};
    DebugState debug;
};

SidecarState s_state;

int sourceIndex(Source source) {
    return static_cast<int>(source);
}

bool isValidSource(Source source) {
    const int index = sourceIndex(source);
    return index >= 0 && index < kSourceCount;
}

PlayerSlot normalizePresenterSlot(PlayerSlot slot) {
    if (slot == PlayerSlot::Secondary && camera::isSplitScreenEnabled() &&
        camera::isSecondaryCameraReady() && getPlayer(slot) != nullptr)
    {
        return slot;
    }
    return PlayerSlot::Primary;
}

int windowIndexForSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Secondary ? camera::kSecondaryWindowId : 0;
}

void refreshDebugState(Transition transition, Source source) {
    s_state.debug.totalDepth = s_state.entryCount;
    s_state.debug.wolfHowlDepth = s_state.depths[sourceIndex(Source::WolfHowl)];
    s_state.debug.itemRingDepth = s_state.depths[sourceIndex(Source::ItemRing)];
    s_state.debug.pauseMenuDepth = s_state.depths[sourceIndex(Source::PauseMenu)];
    s_state.debug.fieldMapDepth = s_state.depths[sourceIndex(Source::FieldMap)];
    s_state.debug.dungeonMapDepth = s_state.depths[sourceIndex(Source::DungeonMap)];
    s_state.debug.agithaInsectDepth = s_state.depths[sourceIndex(Source::AgithaInsect)];
    s_state.debug.lastTransition = transition;
    s_state.debug.lastSource = source;
    s_state.debug.fullscreen = s_state.debug.totalDepth != 0;
    s_state.debug.presenterSlot = PlayerSlot::Primary;
    s_state.debug.presenterWindowIndex = 0;
    s_state.debug.hideNonPresenterVisuals = false;
    if (s_state.entryCount != 0) {
        const Options& options = s_state.entries[s_state.entryCount - 1].options;
        s_state.debug.presenterSlot = options.fullscreenSlot;
        s_state.debug.presenterWindowIndex = windowIndexForSlot(options.fullscreenSlot);
        s_state.debug.hideNonPresenterVisuals = options.hideNonPresenterVisuals;
    }
    s_state.debug.revision++;
}

}  // namespace

void begin(Source source, const Options& options) {
    if (!isValidSource(source) || s_state.entryCount >= kMaxActiveEntries) {
        return;
    }

    const bool wasFullscreen = isFullscreen();
    const int previousWindowIndex = presenterWindowIndex();
    Options normalizedOptions = options;
    normalizedOptions.fullscreenSlot = normalizePresenterSlot(options.fullscreenSlot);
    s_state.entries[s_state.entryCount++] = {source, normalizedOptions};
    s_state.depths[sourceIndex(source)]++;
    refreshDebugState(Transition::Begin, source);
    if (wasFullscreen != isFullscreen() || previousWindowIndex != presenterWindowIndex()) {
        camera::refreshWindowLayout();
    }
}

void end(Source source) {
    if (!isValidSource(source)) {
        return;
    }

    int entryIndex = -1;
    for (int i = s_state.entryCount - 1; i >= 0; i--) {
        if (s_state.entries[i].source == source) {
            entryIndex = i;
            break;
        }
    }
    if (entryIndex < 0) {
        return;
    }

    const bool wasFullscreen = isFullscreen();
    const int previousWindowIndex = presenterWindowIndex();
    for (int i = entryIndex; i < s_state.entryCount - 1; i++) {
        s_state.entries[i] = s_state.entries[i + 1];
    }
    s_state.entryCount--;
    s_state.depths[sourceIndex(source)]--;
    refreshDebugState(Transition::End, source);
    if (wasFullscreen != isFullscreen() || previousWindowIndex != presenterWindowIndex()) {
        camera::refreshWindowLayout();
    }
}

void reset() {
    const bool wasFullscreen = isFullscreen();
    const int previousWindowIndex = presenterWindowIndex();
    s_state.entryCount = 0;
    for (int i = 0; i < kSourceCount; i++) {
        s_state.depths[i] = 0;
    }
    refreshDebugState(Transition::Reset, Source::WolfHowl);
    if (wasFullscreen || previousWindowIndex != presenterWindowIndex()) {
        camera::refreshWindowLayout();
    }
}

bool isFullscreen() {
    return s_state.debug.fullscreen;
}

bool shouldPresentSplitViewports() {
    return camera::isSplitScreenEnabled() && !isFullscreen();
}

bool shouldDrawWindow(int windowIndex) {
    return !isFullscreen() || windowIndex == presenterWindowIndex();
}

bool shouldHideSlot(PlayerSlot slot) {
    return s_state.debug.hideNonPresenterVisuals && slot != PlayerSlot::Invalid &&
           slot != presenterSlot();
}

PlayerSlot presenterSlot() {
    return s_state.debug.presenterSlot;
}

int presenterWindowIndex() {
    return s_state.debug.presenterWindowIndex;
}

const DebugState& getDebugState() {
    return s_state.debug;
}

const char* sourceName(Source source) {
    switch (source) {
    case Source::WolfHowl:
        return "wolf_howl";
    case Source::ItemRing:
        return "item_ring";
    case Source::PauseMenu:
        return "pause_menu";
    case Source::FieldMap:
        return "field_map";
    case Source::DungeonMap:
        return "dungeon_map";
    case Source::AgithaInsect:
        return "agitha_insect";
    default:
        return "unknown";
    }
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

}  // namespace dusk::coop::event_presentation
