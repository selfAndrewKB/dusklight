#pragma once

#include "dolphin/types.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop::world_switch_probe {

struct WorldSwitchRecord {
    u64 eventId = 0;
    u32 simFrame = 0;
    uintptr_t sourceActor = 0;
    int sourceActorId = -1;
    int sourceProfile = -1;
    int sourceRoom = -1;
    int switchNo = -1;
    int roomNo = -1;
    bool wasOnBefore = false;
    bool actorSource = false;
    const char* source = nullptr;
};

struct WorldSwitchDebugState {
    WorldSwitchRecord records[64] = {};
    int recordCount = 0;
    u32 currentSimFrame = 0;
};

void advanceWorldSwitchProbeFrame(u32 frame);
void recordSwitchOn(const fopAc_ac_c* sourceActor, int switchNo, int roomNo, bool wasOnBefore,
                    const char* source);
void suppressNextDirectSwitchOn(int switchNo, int roomNo);

const WorldSwitchDebugState& getWorldSwitchDebugState();

}  // namespace dusk::coop::world_switch_probe
