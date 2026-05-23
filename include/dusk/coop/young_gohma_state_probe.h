#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop::young_gohma_state_probe {

struct YoungGohmaStateProbe {
    u64 eventId = 0;
    u32 simFrame = 0;
    uintptr_t actor = 0;
    int actorId = 0;
    int action = 0;
    int subAction = 0;
    int bck = -1;
    f32 animFrame = 0.0f;
    f32 playSpeed = 0.0f;
    f32 speedF = 0.0f;
    PlayerSlot targetSlot = PlayerSlot::Invalid;
    bool targetFound = false;
    f32 targetDistance = 0.0f;
    s16 targetAngleY = 0;
    s16 angleDiff = 0;
    f32 checkRange = 0.0f;
    s16 checkAngle = 0;
    bool rangeGate = false;
    bool angleGate = false;
    bool losClear = false;
    bool plCheck = false;
    bool attackColliderActive = false;
    bool loopSuspect = false;
    u32 stateRunFrames = 0;
    const char* label = nullptr;
};

struct YoungGohmaStateProbeDebugState {
    YoungGohmaStateProbe probes[16] = {};
    int probeCount = 0;
    u32 currentSimFrame = 0;
};

void advanceYoungGohmaStateProbeFrame(u32 frame);
void recordYoungGohmaStateProbe(const YoungGohmaStateProbe& probe);
void clearYoungGohmaStateProbe(fopAc_ac_c* actor);

const YoungGohmaStateProbeDebugState& getYoungGohmaStateProbeDebugState();

}  // namespace dusk::coop::young_gohma_state_probe
