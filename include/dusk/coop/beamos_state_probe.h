#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop::beamos_state_probe {

struct BeamosWakeCandidate {
    PlayerSlot slot = PlayerSlot::Invalid;
    bool available = false;
    bool eligible = false;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
    u32 failureFlags = 0;
};

struct BeamosStateProbe {
    u64 eventId = 0;
    u32 simFrame = 0;
    uintptr_t actor = 0;
    int actorId = 0;
    int room = 0;
    int argument = 0;
    int action = 0;
    int mode = 0;
    int warningTimer = 0;
    int activationSwitch = 0xff;
    int eyeSwitch = 0xff;
    int bodySwitch = 0xff;
    bool activationSwitchOn = false;
    bool eyeSwitchOn = false;
    bool bodySwitchOn = false;
    bool wakeCheckRan = false;
    bool wakeFound = false;
    PlayerSlot wakeSlot = PlayerSlot::Invalid;
    u32 lastWakeCheckFrame = 0;
    BeamosWakeCandidate candidates[kPlayerSlotCount] = {};
};

struct BeamosStateProbeDebugState {
    BeamosStateProbe probes[16] = {};
    int probeCount = 0;
    u32 currentSimFrame = 0;
};

void advanceBeamosStateProbeFrame(u32 frame);
void recordBeamosState(const BeamosStateProbe& probe);
void recordBeamosWakeCheck(fopAc_ac_c* actor, bool found, PlayerSlot slot,
                           const BeamosWakeCandidate* candidates);
void clearBeamosStateProbe(fopAc_ac_c* actor);

const BeamosStateProbeDebugState& getBeamosStateProbeDebugState();

}  // namespace dusk::coop::beamos_state_probe
