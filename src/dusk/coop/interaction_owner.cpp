#include "dusk/coop/interaction_owner.h"

#include "SSystem/SComponent/c_angle.h"
#include "SSystem/SComponent/c_lib.h"
#include "dusk/coop/player_query.h"
#include "f_op/f_op_actor.h"
#include "m_Do/m_Do_mtx.h"

#include <cmath>

namespace dusk::coop::interaction_owner {
namespace {

constexpr s16 kDoorFacingThreshold = 0x5000;

bool isBetterCandidate(const PromptOwnerResult& result, f32 distance) {
    return !result.found || distance < result.distance;
}

bool acceptsFacing(s16 promptFacing, const fopAc_ac_c* player) {
    return std::abs(static_cast<s16>(promptFacing - player->current.angle.y)) >= kDoorFacingThreshold;
}

}  // namespace

PromptOwnerResult selectOrientedBoxPrompt(const cXyz& origin, s16 angleY, int currentSide,
                                          f32 halfWidth, f32 halfDepth, f32 maxDistance) {
    PromptOwnerResult result;
    result.side = currentSide;

    forEachActivePlayer([&](PlayerSlot slot, fopAc_ac_c* player) {
        cXyz local = player->current.pos - origin;
        mDoMtx_stack_c::YrotS(-angleY);
        mDoMtx_stack_c::multVec(&local, &local);

        const f32 distance = local.abs();
        if (distance > maxDistance || std::fabs(local.x) > halfWidth ||
            std::fabs(local.z) > halfDepth)
        {
            return;
        }

        const int side = local.z > 0.0f ? 0 : 1;
        s16 facing = angleY;
        if (side == 1) {
            facing += 0x7fff;
        }

        if (acceptsFacing(facing, player) && isBetterCandidate(result, distance)) {
            result.slot = slot;
            result.actor = player;
            result.side = side;
            result.distance = distance;
            result.found = true;
        }
    });

    return result;
}

PromptOwnerResult selectProjectedPrompt(const cXyz& origin, const cXyz& forward, s16 angleY,
                                        int currentSide, bool chooseSideFromAngle,
                                        f32 sideLimit, f32 forwardLimit, f32 maxDistanceSq) {
    PromptOwnerResult result;
    result.side = currentSide;

    forEachActivePlayer([&](PlayerSlot slot, fopAc_ac_c* player) {
        cXyz pos = player->attention_info.position;
        pos.y = player->current.pos.y;
        cXyz delta = pos - origin;
        const f32 distanceSq = delta.abs2XZ();
        if (distanceSq > maxDistanceSq) {
            return;
        }

        delta.normalize();
        f32 projected = delta.inprodXZ(forward);
        projected = projected * (distanceSq * projected);
        if (projected > forwardLimit || distanceSq - projected > sideLimit) {
            return;
        }

        int side = currentSide;
        if (chooseSideFromAngle) {
            cSGlobe globe(player->current.pos - origin);
            cSAngle angle;
            angle = globe.U() - angleY;
            side = angle.Abs() < 0x4000 ? 0 : 1;
        }

        s16 facing = angleY;
        if (side == 1) {
            facing += 0x7fff;
        }

        if (acceptsFacing(facing, player) && isBetterCandidate(result, distanceSq)) {
            result.slot = slot;
            result.actor = player;
            result.side = side;
            result.distance = distanceSq;
            result.found = true;
        }
    });

    return result;
}

}  // namespace dusk::coop::interaction_owner
