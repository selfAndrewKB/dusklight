#include "dusk/coop/retained_interaction_owner.h"

#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor.h"

#include <vector>

namespace dusk::coop::retained_interaction_owner {
namespace {

struct RetainedState {
    fopAc_ac_c* owner = nullptr;
    RetainedInteractionScope scope = RetainedInteractionScope::Attach;
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* player = nullptr;
    fopAc_ac_c* playerActor = nullptr;
    RetainedInteractionReason reason = RetainedInteractionReason::Cleared;
    bool active = false;
};

std::vector<RetainedState> s_states;

bool isValidSlot(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid && static_cast<unsigned char>(slot) < kPlayerSlotCount;
}

RetainedState* findState(fopAc_ac_c* owner, RetainedInteractionScope scope, bool create) {
    if (owner == nullptr) {
        return nullptr;
    }

    for (RetainedState& state : s_states) {
        if (state.owner == owner && state.scope == scope) {
            return &state;
        }
    }

    if (!create) {
        return nullptr;
    }

    s_states.push_back({});
    RetainedState& state = s_states.back();
    state.owner = owner;
    state.scope = scope;
    return &state;
}

RetainedInteractionState publicState(const RetainedState* state) {
    RetainedInteractionState result;
    if (state == nullptr) {
        return result;
    }

    result.owner = state->owner;
    result.scope = state->scope;
    result.slot = state->slot;
    result.localPlayer = state->player;
    result.localPlayerActor = state->playerActor;
    result.reason = state->reason;
    result.active = state->active;
    result.found = state->active && isValidSlot(state->slot) && state->player != nullptr;
    return result;
}

void clearState(RetainedState* state) {
    if (state == nullptr) {
        return;
    }

    state->slot = PlayerSlot::Invalid;
    state->player = nullptr;
    state->playerActor = nullptr;
    state->reason = RetainedInteractionReason::Cleared;
    state->active = false;
}

}  // namespace

RetainedInteractionState beginRetainedInteraction(const char* label, fopAc_ac_c* owner,
                                                  RetainedInteractionScope scope,
                                                  fopAc_ac_c* playerActor,
                                                  RetainedInteractionReason reason) {
    (void)label;
    RetainedState* state = findState(owner, scope, true);
    if (state == nullptr) {
        return RetainedInteractionState{};
    }

    PlayerSlot slot = getSlotForActor(playerActor);
    if (!isValidSlot(slot)) {
        slot = PlayerSlot::Primary;
        playerActor = getPrimaryPlayer();
        reason = RetainedInteractionReason::FallbackPrimary;
    }

    state->owner = owner;
    state->scope = scope;
    state->slot = slot;
    state->playerActor = playerActor;
    state->player = static_cast<daPy_py_c*>(playerActor);
    state->reason = reason;
    state->active = state->player != nullptr;
    return publicState(state);
}

RetainedInteractionState updateRetainedInteraction(const char* label, fopAc_ac_c* owner,
                                                   RetainedInteractionScope scope) {
    (void)label;
    RetainedState* state = findState(owner, scope, false);
    if (state == nullptr) {
        return RetainedInteractionState{};
    }

    if (state->active && (!isValidSlot(state->slot) || getPlayer(state->slot) != state->playerActor)) {
        clearState(state);
    }

    return publicState(state);
}

RetainedInteractionState getRetainedInteraction(fopAc_ac_c* owner,
                                                RetainedInteractionScope scope) {
    return publicState(findState(owner, scope, false));
}

void clearRetainedInteraction(const char* label, fopAc_ac_c* owner,
                              RetainedInteractionScope scope) {
    (void)label;
    clearState(findState(owner, scope, false));
}

void clearAllRetainedInteractions(fopAc_ac_c* owner) {
    if (owner == nullptr) {
        return;
    }

    for (RetainedState& state : s_states) {
        if (state.owner == owner) {
            clearState(&state);
        }
    }
}

int countRetainedInteractions(PlayerSlot slot, RetainedInteractionScope scope) {
    int count = 0;
    if (!isValidSlot(slot)) {
        return count;
    }

    for (const RetainedState& state : s_states) {
        if (state.active && state.scope == scope && state.slot == slot &&
            getPlayer(slot) == state.playerActor)
        {
            count++;
        }
    }

    return count;
}

const char* retainedInteractionReasonName(RetainedInteractionReason reason) {
    switch (reason) {
    case RetainedInteractionReason::DirectPlayer:
        return "DirectPlayer";
    case RetainedInteractionReason::EnemyTarget:
        return "EnemyTarget";
    case RetainedInteractionReason::FallbackPrimary:
        return "FallbackPrimary";
    case RetainedInteractionReason::Cleared:
    default:
        return "Cleared";
    }
}

}  // namespace dusk::coop::retained_interaction_owner
