#include "dusk/coop/player_attention.h"

#include "d/actor/d_a_alink.h"
#include "d/d_attention.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/player_slots.h"

#include <new>

namespace dusk::coop::player_attention {
namespace {

struct SlotAttention {
    alignas(dAttention_c) unsigned char storage[sizeof(dAttention_c)];
    dAttention_c* attention = nullptr;
    daAlink_c* player = nullptr;
};

SlotAttention s_attention[kPlayerSlotCount];

dAttention_c* constructAttention(SlotAttention* state, daAlink_c* player, PlayerSlot slot) {
    if (state->attention == nullptr) {
        // Co-op: extra players need their own vanilla scanner state keyed to their pad.
        state->attention = new (state->storage) dAttention_c(player, getPadForSlot(slot));
    } else if (state->player != player) {
        state->attention->Init(player, getPadForSlot(slot));
        state->attention->initList(0xFFFFFFFF);
    }

    state->player = player;
    return state->attention;
}

}  // namespace

dAttention_c* attentionForPlayer(daAlink_c* player) {
    const PlayerSlot slot = getSlotForActor(player);
    if (slot == PlayerSlot::Invalid || slot == PlayerSlot::Primary) {
        return dComIfGp_getAttention();
    }

    const int slotIndex = static_cast<int>(slot);
    if (slotIndex < 0 || slotIndex >= kPlayerSlotCount) {
        return dComIfGp_getAttention();
    }

    return constructAttention(&s_attention[slotIndex], player, slot);
}

void updateForPlayer(daAlink_c* player) {
    dAttention_c* attention = attentionForPlayer(player);
    if (attention == nullptr || attention == dComIfGp_getAttention()) {
        return;
    }

    attention->Run();
}

bool isLockOn(daAlink_c* player) {
    dAttention_c* attention = attentionForPlayer(player);
    return attention != nullptr && attention->Lockon();
}

}  // namespace dusk::coop::player_attention
