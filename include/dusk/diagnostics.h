#pragma once

#include "dolphin/types.h"

#include <cstdint>
#include <filesystem>

namespace dusk::diagnostics {

struct SecondaryAlinkState {
    const char* phase = nullptr;
    uintptr_t actor = 0;
    uintptr_t target = 0;
    uintptr_t anim = 0;
    uintptr_t modelUser = 0;
    uintptr_t ownerUnder = 0;
    uintptr_t ownerUpper = 0;
    uintptr_t itemActor = 0;
    uintptr_t rideActor = 0;
    uintptr_t throwBoomerangActor = 0;
    uintptr_t copyRodActor = 0;
    uintptr_t copyRodControlActor = 0;
    uintptr_t copyRodCameraActor = 0;
    int itemActorId = 0;
    int rideActorId = 0;
    s16 itemActorName = 0;
    s16 rideActorName = 0;
    u16 proc = 0;
    u16 equipItem = 0;
    u8 selectItemId = 0;
    u8 rideStatus = 0;
    u8 itemButton = 0;
    u8 itemTrigger = 0;
    u8 useButtonFlags = 0;
    u8 previousUseButtonFlags = 0;
    s16 stickAngle = 0;
    s16 moveAngle = 0;
    s16 currentAngleY = 0;
    s16 shapeAngleY = 0;
    u32 attentionFlags = 0;
    u32 rawMask = 0;
    u8 rStatus = 0;
    f32 speedF = 0.0f;
    f32 normalSpeed = 0.0f;
    f32 stickValue = 0.0f;
    f32 moveValue = 0.0f;
    f32 posX = 0.0f;
    f32 posY = 0.0f;
    f32 posZ = 0.0f;
    f32 underFrame = 0.0f;
    f32 underRate = 0.0f;
    bool inputR = false;
    bool attentionLock = false;
    bool itemButtonR = false;
    bool itemTriggerR = false;
    bool copyRodTopUse = false;
};

void setSecondaryAlinkActionMirrorProfileEnabled(bool enabled);
bool isSecondaryAlinkActionMirrorProfileEnabled();

void tick(u32 frame);
void flush(const char* reason);

void recordSecondaryAlinkState(const char* phase, const SecondaryAlinkState& state);

const std::filesystem::path& getOutputPath();

}  // namespace dusk::diagnostics
