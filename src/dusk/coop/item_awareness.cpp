#include "dusk/coop/item_awareness.h"

#include "SSystem/SComponent/c_xyz.h"
#include "d/actor/d_a_alink.h"
#include "f_op/f_op_actor_mng.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace dusk::coop::item_awareness {
namespace {

ItemAwarenessDebugState s_debugState;
u64 s_nextEventId = 1;
u32 s_currentSimFrame = 0;
int s_nextDebugEvict = 0;

constexpr int kDecisionCount = sizeof(s_debugState.decisions) / sizeof(s_debugState.decisions[0]);
constexpr int kLabelSize = 64;

void copyLabel(char* dst, const char* label) {
    std::snprintf(dst, kLabelSize, "%s", label != nullptr ? label : "");
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

f32 distanceXZ(const cXyz& a, const cXyz& b) {
    const f32 dx = a.x - b.x;
    const f32 dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

ItemAwarenessDecisionDebug* findDecision(const char* label, const fopAc_ac_c* observer) {
    const char* labelName = label != nullptr ? label : "";
    for (int i = 0; i < s_debugState.decisionCount; i++) {
        ItemAwarenessDecisionDebug& decision = s_debugState.decisions[i];
        if (std::strcmp(decision.label, labelName) == 0 &&
            decision.result.observerDebug.ptr == reinterpret_cast<uintptr_t>(observer))
        {
            return &decision;
        }
    }

    if (s_debugState.decisionCount < kDecisionCount) {
        return &s_debugState.decisions[s_debugState.decisionCount++];
    }
    return &s_debugState.decisions[s_nextDebugEvict++ % kDecisionCount];
}

void recordDecision(const char* label, const fopAc_ac_c* observer,
                    const ItemAwarenessResult& result) {
    ItemAwarenessDecisionDebug* decision = findDecision(label, observer);
    if (decision == nullptr) {
        return;
    }

    const bool changed = decision->result.found != result.found ||
                         decision->result.slot != result.slot ||
                         decision->result.itemActor != result.itemActor ||
                         decision->result.localPlayerActor != result.localPlayerActor ||
                         decision->result.reason != result.reason;

    copyLabel(decision->label, label);
    decision->simFrame = s_currentSimFrame;
    decision->result = result;
    if (changed) {
        decision->eventId = s_nextEventId++;
    }
}

}  // namespace

void advanceItemAwarenessFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

ItemAwarenessResult findActiveBoomerang(const fopAc_ac_c* observer, const char* label) {
    ItemAwarenessResult result;
    result.observerDebug = actorDebug(observer);

    forEachActivePlayer([&](PlayerSlot slot, fopAc_ac_c* actor) {
        if (actor == nullptr || fopAcM_GetName(actor) != fpcNm_ALINK_e) {
            return;
        }

        daAlink_c* player = static_cast<daAlink_c*>(actor);
        // Co-op: boomerang awareness reads the owner-local human ALINK item keep. Wolf slots
        // cannot own an active boomerang actor and must not be cast through human-only state.
        if (player->checkWolf() || !player->checkBoomerangChargeEnd()) {
            return;
        }

        fopAc_ac_c* boomerang = player->getBoomerangActor();
        if (boomerang == nullptr || fopAcM_GetName(boomerang) != fpcNm_BOOMERANG_e ||
            fopAcM_GetParam(boomerang) != 1)
        {
            return;
        }

        const f32 candidateDistance = observer != nullptr
                                          ? distanceXZ(observer->current.pos, boomerang->current.pos)
                                          : 0.0f;
        if (result.found && candidateDistance >= result.distanceXZ) {
            return;
        }

        result.slot = slot;
        result.localPlayerActor = actor;
        result.localPlayer = player;
        result.itemActor = boomerang;
        result.playerDebug = actorDebug(actor);
        result.itemDebug = actorDebug(boomerang);
        result.distanceXZ = candidateDistance;
        result.reason = ItemAwarenessReason::OwnedBoomerang;
        result.found = true;
    });

    recordDecision(label, observer, result);
    return result;
}

const ItemAwarenessDebugState& getItemAwarenessDebugState() {
    return s_debugState;
}

const char* itemAwarenessReasonName(ItemAwarenessReason reason) {
    switch (reason) {
    case ItemAwarenessReason::OwnedBoomerang:
        return "OwnedBoomerang";
    case ItemAwarenessReason::NoMatch:
    default:
        return "NoMatch";
    }
}

}  // namespace dusk::coop::item_awareness
