#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

namespace dusk::coop::hud_diagnostics {

enum class ReplayPhase {
    BeforeSecondaryApply,
    SecondaryApplied,
    PrimaryRestored,
    Count,
};

enum class RingAdmissionPhase {
    Request,
    PromptCleanup,
    Create,
};

struct ItemResolverDebug {
    u8 selectIndex = 0xff;
    u8 mixIndex = 0xff;
    u8 item = 0xff;
    s16 count = 0;
    int maxCount = 0;
};

struct ItemPaneDebug {
    bool visible = false;
    bool textureVisible = false;
    bool thirdDigitVisible = false;
    u8 alpha = 0;
    f32 alphaRate = 0.0f;
    f32 translateX = 0.0f;
    f32 translateY = 0.0f;
    f32 scaleX = 0.0f;
    f32 scaleY = 0.0f;
};

struct ReplaySnapshot {
    bool valid = false;
    ReplayPhase phase = ReplayPhase::BeforeSecondaryApply;
    PlayerSlot presentationSlot = PlayerSlot::Invalid;
    u8 doStatus = 0;
    ItemResolverDebug resolved[2];
    ItemPaneDebug panes[2];
};

struct HudPresentationDebugState {
    unsigned int revision = 0;
    ItemResolverDebug slotItems[2][2];
    ReplaySnapshot snapshots[static_cast<int>(ReplayPhase::Count)];
    struct RingAdmissionDebug {
        bool valid = false;
        RingAdmissionPhase phase = RingAdmissionPhase::Request;
        PlayerSlot owner = PlayerSlot::Invalid;
        u8 heapLock = 0;
        u8 subHeapLocks[2] = {};
        bool primaryPrompt = false;
        bool secondaryPrompt = false;
        u8 messageStatus = 0;
        bool floatingMessageVisible = false;
    } ringAdmission;
};

void recordSnapshot(const ReplaySnapshot& snapshot);
void recordRingAdmission(RingAdmissionPhase phase, PlayerSlot owner, bool primaryPrompt,
                         bool secondaryPrompt);
const HudPresentationDebugState& getState();

bool isOverlayEnabled();
void setOverlayEnabled(bool enabled);
void drawTextOverlay();

const char* replayPhaseName(ReplayPhase phase);
const char* ringAdmissionPhaseName(RingAdmissionPhase phase);

}  // namespace dusk::coop::hud_diagnostics
