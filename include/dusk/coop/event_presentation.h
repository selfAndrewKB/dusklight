#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

namespace dusk::coop::event_presentation {

enum class Source : u8 {
    WolfHowl = 0,
    ItemRing,
    PauseMenu,
    FieldMap,
    DungeonMap,
    AgithaInsect,
    MidnaService,
    Dialogue,
    EnemyRetainedInteraction,
    Count,
};

enum class Transition : u8 {
    None = 0,
    Begin,
    End,
    Reset,
};

struct Options {
    PlayerSlot fullscreenSlot = PlayerSlot::Primary;
    bool hideNonPresenterVisuals = true;
};

struct DebugState {
    u32 revision = 0;
    int totalDepth = 0;
    int wolfHowlDepth = 0;
    int itemRingDepth = 0;
    int pauseMenuDepth = 0;
    int fieldMapDepth = 0;
    int dungeonMapDepth = 0;
    int agithaInsectDepth = 0;
    int midnaServiceDepth = 0;
    int dialogueDepth = 0;
    int enemyRetainedInteractionDepth = 0;
    Transition lastTransition = Transition::None;
    Source lastSource = Source::WolfHowl;
    PlayerSlot presenterSlot = PlayerSlot::Primary;
    int presenterWindowIndex = 0;
    bool fullscreen = false;
    bool hideNonPresenterVisuals = false;
};

void begin(Source source, const Options& options = {});
void end(Source source);
void reset();

bool isFullscreen();
bool shouldPresentSplitViewports();
bool shouldDrawWindow(int windowIndex);
bool shouldHideSlot(PlayerSlot slot);
PlayerSlot presenterSlot();
int presenterWindowIndex();

const DebugState& getDebugState();
const char* sourceName(Source source);
const char* transitionName(Transition transition);

}  // namespace dusk::coop::event_presentation
