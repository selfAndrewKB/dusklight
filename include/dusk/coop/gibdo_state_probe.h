#pragma once

#include "dolphin/types.h"
#include "dusk/coop/enemy_targeting.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop::gibdo_state_probe {

struct GibdoStateProbe {
    u64 eventId = 0;
    u32 simFrame = 0;
    uintptr_t actor = 0;
    uintptr_t cryOwner = 0;
    int actorId = 0;
    int action = 0;
    int moveMode = 0;
    int bck = -1;
    f32 animFrame = 0.0f;
    f32 playSpeed = 0.0f;
    f32 speedF = 0.0f;
    int attackDelay = 0;
    int stunTimer = 0;
    int cryTimer = 0;
    PlayerSlot targetSlot = PlayerSlot::Invalid;
    bool targetFound = false;
    f32 targetDistance = 0.0f;
    s16 targetAngleY = 0;
    bool rangeGate = false;
    bool angleGate = false;
    bool losClear = false;
    bool delayGate = false;
    bool attackGate = false;
    bool attackStart = false;
    bool screamOwnerActive = false;
    bool screamOwnerAttackStarted = false;
    bool loopSuspect = false;
    u32 stateRunFrames = 0;
    const char* label = nullptr;
};

struct GibdoStateProbeDebugState {
    GibdoStateProbe probes[16] = {};
    int probeCount = 0;
    u32 currentSimFrame = 0;
};

void advanceGibdoStateProbeFrame(u32 frame);
void noteGibdoBck(fopAc_ac_c* actor, int bck);
int currentGibdoBck(fopAc_ac_c* actor);
void recordGibdoStateProbe(const GibdoStateProbe& probe);
void clearGibdoStateProbe(fopAc_ac_c* actor);

const GibdoStateProbeDebugState& getGibdoStateProbeDebugState();

}  // namespace dusk::coop::gibdo_state_probe
