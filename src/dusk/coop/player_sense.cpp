#include "dusk/coop/player_sense.h"

#include "d/actor/d_a_alink.h"
#include "SSystem/SComponent/c_counter.h"
#include "dusk/coop/render_effects.h"
#include "f_op/f_op_actor_mng.h"

namespace dusk::coop::player_sense {
namespace {

constexpr int kRevealActorCapacity = 128;

struct RevealActorEntry {
    const fopAc_ac_c* actor = nullptr;
    fpc_ProcID processId = fpcM_ERROR_PROCESS_ID_e;
    unsigned int lastSeenFrame = 0;
};

RevealActorEntry s_revealActors[kRevealActorCapacity] = {};

bool isLiveEntry(RevealActorEntry& entry) {
    if (entry.actor == nullptr || entry.processId == fpcM_ERROR_PROCESS_ID_e) {
        return false;
    }

    fopAc_ac_c* liveActor = nullptr;
    fopAcM_SearchByID(entry.processId, &liveActor);
    if (liveActor != entry.actor) {
        entry = {};
        return false;
    }

    return true;
}

unsigned int currentFrame() {
    return static_cast<unsigned int>(g_Counter.mCounter0);
}

bool isCurrentFrame(unsigned int registeredFrame) {
    const unsigned int frame = currentFrame();
    return registeredFrame == frame || registeredFrame + 1 == frame;
}

PlayerQueryEligibility revealPredicate(PlayerSlot slot, fopAc_ac_c*, void*) {
    PlayerQueryEligibility eligibility;
    eligibility.eligible = isRevealReady(slot);
    if (!eligibility.eligible) {
        eligibility.failureFlags = PlayerQueryEligibilityFailure_Form |
                                   PlayerQueryEligibilityFailure_Status;
    }
    return eligibility;
}

}  // namespace

bool isActive(PlayerSlot slot) {
    daAlink_c* player = static_cast<daAlink_c*>(getPlayer(slot));
    return player != nullptr && player->checkWolfEyeUp() != 0;
}

bool isRevealReady(PlayerSlot slot) {
    // Co-op: preserve the native reveal threshold independently for each ALINK Sense fade.
    return isActive(slot) && render_effects::senseStrength(slot) > 0.6f;
}

bool anyRevealReady() {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        if (isRevealReady(static_cast<PlayerSlot>(i))) {
            return true;
        }
    }
    return false;
}

PlayerQueryResult findNearestRevealPlayer(const fopAc_ac_c* observer, const char* system) {
    return findNearestPlayerMatching(observer, system, revealPredicate, nullptr);
}

void registerRevealActor(const fopAc_ac_c* actor) {
    if (actor == nullptr) {
        return;
    }

    RevealActorEntry* freeEntry = nullptr;
    for (RevealActorEntry& entry : s_revealActors) {
        if (entry.actor == actor) {
            entry.processId = fopAcM_GetID(actor);
            entry.lastSeenFrame = currentFrame();
            return;
        }
        if ((!isLiveEntry(entry) || !isCurrentFrame(entry.lastSeenFrame)) && freeEntry == nullptr) {
            freeEntry = &entry;
        }
    }

    if (freeEntry != nullptr) {
        freeEntry->actor = actor;
        freeEntry->processId = fopAcM_GetID(actor);
        freeEntry->lastSeenFrame = currentFrame();
    }
}

bool requiresSense(const fopAc_ac_c* actor) {
    if (actor == nullptr) {
        return false;
    }

    for (RevealActorEntry& entry : s_revealActors) {
        if (entry.actor == actor) {
            return isLiveEntry(entry) && isCurrentFrame(entry.lastSeenFrame);
        }
    }
    return false;
}

bool canReveal(PlayerSlot slot, const fopAc_ac_c* actor) {
    return !requiresSense(actor) || isRevealReady(slot);
}

void reset() {
    for (RevealActorEntry& entry : s_revealActors) {
        entry = {};
    }
}

}  // namespace dusk::coop::player_sense
