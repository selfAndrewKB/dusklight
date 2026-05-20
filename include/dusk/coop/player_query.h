#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class fopAc_ac_c;
struct cXyz;

namespace dusk::coop {

struct PlayerQueryResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
    bool found = false;
};

struct PlayerQueryActorDebug {
    uintptr_t ptr = 0;
    int profile = 0;
    int id = 0;
    int room = 0;
    int argument = 0;
    f32 pos[3] = {};
    s16 angleY = 0;
    bool available = false;
};

struct PlayerQueryCandidateDebug {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    PlayerQueryActorDebug actorDebug;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
};

struct PlayerQueryDecisionDebug {
    char system[64] = {};
    fopAc_ac_c* observer = nullptr;
    PlayerQueryActorDebug observerDebug;
    PlayerQueryResult selected;
    PlayerQueryActorDebug selectedActorDebug;
    PlayerQueryCandidateDebug candidates[kPlayerSlotCount] = {};
    int candidateCount = 0;
};

struct PlayerQueryDebugState {
    PlayerQueryDecisionDebug decisions[16] = {};
    int decisionCount = 0;
};

template <typename Func>
void forEachActivePlayer(Func fn) {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        PlayerSlot slot = static_cast<PlayerSlot>(i);
        fopAc_ac_c* actor = getPlayer(slot);
        if (actor != nullptr) {
            fn(slot, actor);
        }
    }
}

PlayerQueryResult findNearestPlayer(const fopAc_ac_c* observer, const char* system);
PlayerQueryResult findNearestPlayerToPos(const cXyz& pos, const char* system);

const PlayerQueryDebugState& getPlayerQueryDebugState();

}  // namespace dusk::coop
