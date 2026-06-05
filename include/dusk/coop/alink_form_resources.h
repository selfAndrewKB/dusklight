#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class daAlink_c;

namespace dusk::coop::alink_form_resources {

struct DebugArcState {
    const char* arcName = nullptr;
    uintptr_t heap = 0;
    int retainCount = 0;
    int phaseId = 0;
    bool loaded = false;
};

struct DebugSlotState {
    PlayerSlot slot = PlayerSlot::Invalid;
    uintptr_t actor = 0;
    const char* currentArc = nullptr;
    const char* pendingReleaseArc = nullptr;
    bool desiredWolf = false;
    bool desiredKnown = false;
    bool swapping = false;
};

struct DebugState {
    u32 revision = 0;
    DebugArcState arcs[8];
    int arcCount = 0;
    DebugSlotState slots[kPlayerSlotCount];
};

void applyDesiredFormOnCreate(daAlink_c* player);
int loadInitial(daAlink_c* player, const char* arcName);
void beginSwap(daAlink_c* player, const char* oldArcName);
int loadSwapArc(daAlink_c* player, const char* arcName);
void completeSwap(daAlink_c* player);
bool releaseActor(daAlink_c* player);
void setSlotWolf(daAlink_c* player, bool wolf);
bool canInstallModelDataOwner(daAlink_c* player);
void resetRuntime();

const DebugState& getDebugState();

}  // namespace dusk::coop::alink_form_resources
