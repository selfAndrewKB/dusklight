#pragma once

#include "dolphin/types.h"
#include "dusk/coop/defender_owner.h"
#include "dusk/coop/enemy_targeting.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop::bokoblin_attack_probe {

struct BokoblinAttackProbe {
    u64 eventId = 0;
    u32 simFrame = 0;
    uintptr_t actor = 0;
    int actorId = 0;
    int action = 0;
    int state = 0;
    int bck = -1;
    f32 animFrame = 0.0f;
    f32 playSpeed = 0.0f;
    f32 speedF = 0.0f;
    f32 targetDistance = 0.0f;
    s16 targetAngleY = 0;
    PlayerSlot targetSlot = PlayerSlot::Invalid;
    bool targetFound = false;
    bool attackAnimationStarted = false;
    bool attackActiveWindow = false;
    bool guardedHit = false;
    bool sphere0Hit = false;
    bool sphere1Hit = false;
    bool preActiveWindowHit = false;
    bool preActiveWindowGuarded = false;
    u32 firstHitFrame = 0;
    f32 firstHitAnimFrame = 0.0f;
    bool firstHitBeforeActiveWindow = false;
    bool firstHitGuarded = false;
    defender_owner::DefenderOwnerResult sphere0;
    defender_owner::DefenderOwnerResult sphere1;
    bool loopSuspect = false;
    u32 attackRunFrames = 0;
};

struct BokoblinAttackProbeDebugState {
    BokoblinAttackProbe probes[16] = {};
    int probeCount = 0;
    u32 currentSimFrame = 0;
};

struct BokoblinSteeringProbe {
    u64 eventId = 0;
    u32 simFrame = 0;
    uintptr_t actor = 0;
    int actorId = 0;
    int action = 0;
    int state = 0;
    char label[64] = {};
    PlayerSlot targetSlot = PlayerSlot::Invalid;
    bool targetFound = false;
    f32 targetDistance = 0.0f;
    f32 targetDistanceXZ = 0.0f;
    s16 targetAngleY = 0;
    PlayerSlot nearestSlot = PlayerSlot::Invalid;
    bool nearestFound = false;
    f32 nearestDistance = 0.0f;
    f32 nearestDistanceXZ = 0.0f;
    f32 p1Distance = 0.0f;
    f32 p1DistanceXZ = 0.0f;
    f32 speedF = 0.0f;
    int speedSign = 0;
    s16 shapeAngleY = 0;
    bool detourActive = false;
    bool detourJustSet = false;
    s16 detourTimer = 0;
    s16 detourAngleY = 0;
    bool moveOut = false;
    f32 homeDistance = 0.0f;
    f32 moveRange = 0.0f;
    bool p1CloserThanTarget = false;
};

struct BokoblinSteeringProbeDebugState {
    BokoblinSteeringProbe probes[16] = {};
    int probeCount = 0;
    u32 currentSimFrame = 0;
};

void advanceBokoblinAttackProbeFrame(u32 frame);
void recordBokoblinAttackProbe(const BokoblinAttackProbe& probe);
void recordBokoblinSteeringProbe(const BokoblinSteeringProbe& probe);
void clearBokoblinAttackProbe(fopAc_ac_c* actor);

const BokoblinAttackProbeDebugState& getBokoblinAttackProbeDebugState();
const BokoblinSteeringProbeDebugState& getBokoblinSteeringProbeDebugState();

}  // namespace dusk::coop::bokoblin_attack_probe
