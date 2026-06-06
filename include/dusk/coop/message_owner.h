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

struct DebugState {
    u32 revision = 0;
    bool active = false;
    bool presentationActive = false;
    Transition lastTransition = Transition::None;
    PlayerSlot slot = PlayerSlot::Primary;
    int pad = 0;
    uintptr_t listener = 0;
    uintptr_t speaker = 0;
};

void begin(PlayerSlot slot, fopAc_ac_c* listener, fopAc_ac_c* speaker, bool fullscreen);
void end();
void reset();

bool isActive();
PlayerSlot currentSlot();
int currentPad();
daAlink_c* currentPlayer();
fopAc_ac_c* listener();
fopAc_ac_c* speaker();
bool isPresenterSlot(PlayerSlot slot);

const DebugState& getDebugState();
const char* transitionName(Transition transition);

}  // namespace dusk::coop::message_owner
