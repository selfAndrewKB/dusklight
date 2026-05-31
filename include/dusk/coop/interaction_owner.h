#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class fopAc_ac_c;

namespace dusk::coop::interaction_owner {

struct PromptOwnerResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    int side = 0;
    f32 distance = 0.0f;
    bool found = false;
};

PromptOwnerResult selectOrientedBoxPrompt(const cXyz& origin, s16 angleY, int currentSide,
                                          f32 halfWidth, f32 halfDepth, f32 maxDistance);
PromptOwnerResult selectProjectedPrompt(const cXyz& origin, const cXyz& forward, s16 angleY,
                                        int currentSide, bool chooseSideFromAngle,
                                        f32 sideLimit, f32 forwardLimit, f32 maxDistanceSq);

}  // namespace dusk::coop::interaction_owner
