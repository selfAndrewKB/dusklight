#pragma once

#include "dolphin/types.h"

#include <cstdint>
#include <filesystem>

namespace dusk::diagnostics {

struct SecondaryAlinkState {
    uintptr_t actor = 0;
    uintptr_t target = 0;
    uintptr_t anim = 0;
    uintptr_t modelUser = 0;
    uintptr_t ownerUnder = 0;
    uintptr_t ownerUpper = 0;
    u16 proc = 0;
    u32 attentionFlags = 0;
    u32 rawMask = 0;
    u8 rStatus = 0;
    f32 speedF = 0.0f;
    f32 normalSpeed = 0.0f;
    f32 stickValue = 0.0f;
    f32 underFrame = 0.0f;
    f32 underRate = 0.0f;
    bool inputR = false;
    bool attentionLock = false;
    bool itemButtonR = false;
    bool itemTriggerR = false;
};

void setSecondaryAlinkActionMirrorProfileEnabled(bool enabled);
bool isSecondaryAlinkActionMirrorProfileEnabled();

void tick(u32 frame);
void flush(const char* reason);

void recordSecondaryAlinkState(const char* phase, const SecondaryAlinkState& state);

const std::filesystem::path& getOutputPath();

}  // namespace dusk::diagnostics
