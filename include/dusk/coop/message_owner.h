#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class daAlink_c;
class fopAc_ac_c;

namespace dusk::coop::message_owner {

enum class Transition : u8 {
    None = 0,
    Begin,
    End,
    Reset,
};

enum class BeginSource : u8 {
    None = 0,
    MessageAccept,
    MessageAcceptDemo,
    TalkStartFallback,
    MidnaLocalString,
    MidnaService,
};

struct DebugState {
    u32 revision = 0;
    bool active = false;
    bool presentationActive = false;
    Transition lastTransition = Transition::None;
    BeginSource lastBeginSource = BeginSource::None;
    PlayerSlot slot = PlayerSlot::Primary;
    int pad = 0;
    uintptr_t presenter = 0;
    uintptr_t listener = 0;
    uintptr_t speaker = 0;
    uintptr_t fallbackActor = 0;
    int talkCut = -1;
};

void begin(PlayerSlot slot, fopAc_ac_c* listener, fopAc_ac_c* speaker, bool fullscreen,
           BeginSource source = BeginSource::MessageAccept);
void end();
void reset();

bool isActive();
PlayerSlot currentSlot();
int currentPad();
daAlink_c* currentPlayer();
fopAc_ac_c* presenterActor();
fopAc_ac_c* listener();
fopAc_ac_c* speaker();
bool isPresenterSlot(PlayerSlot slot);
void recordTalkCameraDebug(fopAc_ac_c* fallbackActor, int talkCut);

const DebugState& getDebugState();
const char* transitionName(Transition transition);
const char* beginSourceName(BeginSource source);

}  // namespace dusk::coop::message_owner
