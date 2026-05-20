#include "dusk/coop/player_query.h"

#include "SSystem/SComponent/c_lib.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>

namespace dusk::coop {
namespace {

PlayerQueryDebugState s_debugState;
constexpr int kPlayerQueryDecisionCount =
    sizeof(s_debugState.decisions) / sizeof(s_debugState.decisions[0]);
constexpr int kPlayerQuerySystemNameSize = 64;

void copySystemName(char* dst, const char* system) {
    std::snprintf(dst, kPlayerQuerySystemNameSize, "%s", system != nullptr ? system : "");
}

PlayerQueryActorDebug actorDebug(const fopAc_ac_c* actor) {
    PlayerQueryActorDebug debug;
    if (actor == nullptr) {
        return debug;
    }

    debug.ptr = reinterpret_cast<uintptr_t>(actor);
    debug.profile = static_cast<int>(fopAcM_GetProfName(actor));
    debug.id = static_cast<int>(fopAcM_GetID(actor));
    debug.room = static_cast<int>(fopAcM_GetRoomNo(actor));
    debug.argument = static_cast<int>(actor->argument);
    debug.pos[0] = actor->current.pos.x;
    debug.pos[1] = actor->current.pos.y;
    debug.pos[2] = actor->current.pos.z;
    debug.angleY = actor->shape_angle.y;
    debug.available = true;
    return debug;
}

void recordDecision(const char* system, const fopAc_ac_c* observer,
                    const PlayerQueryResult& selected,
                    const PlayerQueryCandidateDebug* candidates, int candidateCount) {
    const char* systemName = system != nullptr ? system : "";
    PlayerQueryDecisionDebug* decision = nullptr;
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        if (s_debugState.decisions[i].observer == observer &&
            std::strcmp(s_debugState.decisions[i].system, systemName) == 0)
        {
            decision = &s_debugState.decisions[i];
            break;
        }
    }
    if (decision == nullptr) {
        if (s_debugState.decisionCount < kPlayerQueryDecisionCount) {
            decision = &s_debugState.decisions[s_debugState.decisionCount++];
        } else {
            decision = &s_debugState.decisions[0];
        }
    }

    copySystemName(decision->system, systemName);
    decision->observer = const_cast<fopAc_ac_c*>(observer);
    decision->observerDebug = actorDebug(observer);
    decision->selected = selected;
    decision->selectedActorDebug = actorDebug(selected.actor);
    decision->candidateCount = candidateCount;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        decision->candidates[i] = i < candidateCount ? candidates[i] : PlayerQueryCandidateDebug{};
    }
}

PlayerQueryResult chooseNearest(const fopAc_ac_c* observer, const cXyz& pos, const char* system) {
    PlayerQueryResult result;
    PlayerQueryCandidateDebug candidates[kPlayerSlotCount] = {};
    int candidateCount = 0;

    forEachActivePlayer([&](PlayerSlot slot, fopAc_ac_c* actor) {
        PlayerQueryCandidateDebug& candidate = candidates[candidateCount++];
        candidate.slot = slot;
        candidate.actor = actor;
        candidate.actorDebug = actorDebug(actor);
        if (observer != nullptr) {
            candidate.distance = fopAcM_searchActorDistance(observer, actor);
            candidate.distanceXZ = fopAcM_searchActorDistanceXZ(observer, actor);
            candidate.angleY = fopAcM_searchActorAngleY(observer, actor);
        } else {
            cXyz delta = actor->current.pos - pos;
            candidate.distance = delta.abs();
            candidate.distanceXZ = delta.absXZ();
            candidate.angleY = cLib_targetAngleY(pos, actor->current.pos);
        }

        if (!result.found || candidate.distanceXZ < result.distanceXZ) {
            result.slot = candidate.slot;
            result.actor = candidate.actor;
            result.distance = candidate.distance;
            result.distanceXZ = candidate.distanceXZ;
            result.angleY = candidate.angleY;
            result.found = true;
        }
    });

    recordDecision(system, observer, result, candidates, candidateCount);
    return result;
}

}  // namespace

PlayerQueryResult findNearestPlayer(const fopAc_ac_c* observer, const char* system) {
    if (observer == nullptr) {
        return PlayerQueryResult{};
    }

    return chooseNearest(observer, observer->current.pos, system);
}

PlayerQueryResult findNearestPlayerToPos(const cXyz& pos, const char* system) {
    return chooseNearest(nullptr, pos, system);
}

const PlayerQueryDebugState& getPlayerQueryDebugState() {
    return s_debugState;
}

}  // namespace dusk::coop
