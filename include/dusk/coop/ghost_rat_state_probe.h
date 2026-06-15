#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop::ghost_rat_state_probe {

struct GhostRatStateProbe {
    u64 eventId = 0;
    u32 simFrame = 0;
    uintptr_t actor = 0;
    int actorId = 0;
    int action = 0;
    int subAction = 0;
    int bck = -1;
    f32 animFrame = 0.0f;
    PlayerSlot wakeSlot = PlayerSlot::Invalid;
    bool wakeFound = false;
    f32 wakeDistance = 0.0f;
    f32 wakeDistanceXZ = 0.0f;
    s16 wakeAngleY = 0;
    s16 angleDiff = 0;
    f32 checkRange = 0.0f;
    bool rangeGate = false;
    bool coneGate = false;
    bool bodySlotAvailable = false;
    bool attackFrameGate = false;
    bool attackStarted = false;
    int switchNo = -1;
    bool switchGateActive = false;
    bool switchOn = false;
    bool reachedAction = false;
    const char* label = nullptr;
};

struct GhostRatStateProbeDebugState {
    GhostRatStateProbe probes[24] = {};
    int probeCount = 0;
    u32 currentSimFrame = 0;
};

void advanceGhostRatStateProbeFrame(u32 frame);
void recordGhostRatStateProbe(const GhostRatStateProbe& probe);
void clearGhostRatStateProbe(fopAc_ac_c* actor);

const GhostRatStateProbeDebugState& getGhostRatStateProbeDebugState();

}  // namespace dusk::coop::ghost_rat_state_probe
