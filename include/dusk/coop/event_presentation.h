#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

namespace dusk::coop::event_presentation {

enum class Source : u8 {
    WolfHowl = 0,
    Count,
};

enum class Transition : u8 {
    None = 0,
    Begin,
    End,
    Reset,
};

struct Options {
    bool hideAdditionalVisuals = true;
};

struct DebugState {
    u32 revision = 0;
    int totalDepth = 0;
    int wolfHowlDepth = 0;
    Transition lastTransition = Transition::None;
    Source lastSource = Source::WolfHowl;
    bool fullscreen = false;
    bool hideAdditionalVisuals = false;
};

void begin(Source source, const Options& options = {});
void end(Source source);
void reset();

bool isFullscreen();
bool shouldPresentSplitViewports();
bool shouldDrawWindow(int windowIndex);
bool shouldHideSlot(PlayerSlot slot);

const DebugState& getDebugState();
const char* sourceName(Source source);
const char* transitionName(Transition transition);

}  // namespace dusk::coop::event_presentation
