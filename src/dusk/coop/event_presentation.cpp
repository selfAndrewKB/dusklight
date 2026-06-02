#include "dusk/coop/event_presentation.h"

#include "dusk/coop/camera.h"

namespace dusk::coop::event_presentation {
namespace {

constexpr int kSourceCount = static_cast<int>(Source::Count);
constexpr int kMaxSourceDepth = 8;

struct SidecarState {
    Options options[kSourceCount][kMaxSourceDepth];
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

void refreshDebugState(Transition transition, Source source) {
    s_state.debug.totalDepth = 0;
    for (int i = 0; i < kSourceCount; i++) {
        s_state.debug.totalDepth += s_state.depths[i];
    }

    s_state.debug.wolfHowlDepth = s_state.depths[sourceIndex(Source::WolfHowl)];
    s_state.debug.lastTransition = transition;
    s_state.debug.lastSource = source;
    s_state.debug.fullscreen = s_state.debug.totalDepth != 0;
    s_state.debug.hideAdditionalVisuals = false;
    for (int i = 0; i < kSourceCount; i++) {
        for (int depth = 0; depth < s_state.depths[i]; depth++) {
            if (s_state.options[i][depth].hideAdditionalVisuals) {
                s_state.debug.hideAdditionalVisuals = true;
                break;
            }
        }
        if (s_state.debug.hideAdditionalVisuals) {
            break;
        }
    }
    s_state.debug.revision++;
}

}  // namespace

void begin(Source source, const Options& options) {
    if (!isValidSource(source)) {
        return;
    }

    const bool wasFullscreen = isFullscreen();
    const int index = sourceIndex(source);
    if (s_state.depths[index] >= kMaxSourceDepth) {
        return;
    }
    s_state.options[index][s_state.depths[index]++] = options;
    refreshDebugState(Transition::Begin, source);
    if (wasFullscreen != isFullscreen()) {
        camera::refreshWindowLayout();
    }
}

void end(Source source) {
    if (!isValidSource(source)) {
        return;
    }

    const int index = sourceIndex(source);
    if (s_state.depths[index] == 0) {
        return;
    }

    const bool wasFullscreen = isFullscreen();
    s_state.depths[index]--;
    refreshDebugState(Transition::End, source);
    if (wasFullscreen != isFullscreen()) {
        camera::refreshWindowLayout();
    }
}

void reset() {
    const bool wasFullscreen = isFullscreen();
    for (int i = 0; i < kSourceCount; i++) {
        s_state.depths[i] = 0;
    }
    refreshDebugState(Transition::Reset, Source::WolfHowl);
    if (wasFullscreen) {
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
    return !isFullscreen() || windowIndex == 0;
}

bool shouldHideSlot(PlayerSlot slot) {
    return s_state.debug.hideAdditionalVisuals && slot != PlayerSlot::Invalid &&
           slot != PlayerSlot::Primary;
}

const DebugState& getDebugState() {
    return s_state.debug;
}

const char* sourceName(Source source) {
    switch (source) {
    case Source::WolfHowl:
        return "wolf_howl";
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
