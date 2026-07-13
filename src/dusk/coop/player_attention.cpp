#include "dusk/coop/player_attention.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_tag_wljump.h"
#include "d/d_attention.h"
#include "d/d_com_inf_game.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "dusk/coop/player_slots.h"
#include "dusk/coop/player_sense.h"

#include <new>

namespace dusk::coop::player_attention {
namespace {

struct SlotAttention {
    alignas(dAttention_c) unsigned char storage[sizeof(dAttention_c)];
    dAttention_c* attention = nullptr;
    daAlink_c* player = nullptr;
};

SlotAttention s_attention[kPlayerSlotCount];
J3DDrawBuffer* s_viewportCursorBuffer = nullptr;
bool s_viewportCursorBufferReady = false;
bool s_viewportCursorDrawActive = false;

bool ensureViewportCursorBuffer() {
    if (s_viewportCursorBufferReady) {
        return true;
    }

    if (s_viewportCursorBuffer == nullptr) {
        s_viewportCursorBuffer = new J3DDrawBuffer();
        if (s_viewportCursorBuffer == nullptr) {
            return false;
        }
    }

    if (s_viewportCursorBuffer->allocBuffer(4) != kJ3DError_Success) {
        return false;
    }

    s_viewportCursorBuffer->setNonSort();
    s_viewportCursorBufferReady = true;
    return true;
}

dAttention_c* constructAttention(SlotAttention* state, daAlink_c* player, PlayerSlot slot) {
    if (state->attention == nullptr) {
        // Co-op: extra players need their own vanilla scanner state keyed to their pad.
        state->attention = new (state->storage) dAttention_c(player, getPadForSlot(slot));
    } else if (state->player != player) {
        state->attention->~dAttention_c();
        state->attention = new (state->storage) dAttention_c(player, getPadForSlot(slot));
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

dAttention_c* existingAttentionForSlot(int slot) {
    if (slot == 0) {
        return dComIfGp_getAttention();
    }

    if (slot < 0 || slot >= kPlayerSlotCount) {
        return nullptr;
    }

    return s_attention[slot].attention;
}

void updateForPlayer(daAlink_c* player) {
    dAttention_c* attention = attentionForPlayer(player);
    if (attention == nullptr || attention == dComIfGp_getAttention()) {
        return;
    }

    attention->Run();
}

void updateAdditionalPlayers() {
    for (int i = 1; i < kPlayerSlotCount; i++) {
        daAlink_c* player = static_cast<daAlink_c*>(getPlayer(static_cast<PlayerSlot>(i)));
        if (player != nullptr) {
            updateForPlayer(player);
        }
    }
}

bool isLockOn(daAlink_c* player) {
    dAttention_c* attention = attentionForPlayer(player);
    return attention != nullptr && attention->Lockon();
}

fopAc_ac_c* zHintForPlayer(daAlink_c* player) {
    dAttention_c* attention = attentionForPlayer(player);
    return attention != nullptr ? attention->getZHintTarget() : nullptr;
}

int requestZHintForPlayer(daAlink_c* player, fopAc_ac_c* actor, int priority) {
    dAttention_c* attention = attentionForPlayer(player);
    return attention != nullptr ? attention->ZHintRequest(actor, priority) : 0;
}

void drawAll() {
    for (int i = 1; i < kPlayerSlotCount; i++) {
        SlotAttention& state = s_attention[i];
        if (state.attention != nullptr && state.player != nullptr) {
            state.attention->Draw();
        }
    }
}

void drawForCamera(int cameraId) {
    if (!ensureViewportCursorBuffer()) {
        return;
    }

    dAttention_c* attention = existingAttentionForSlot(cameraId);
    if (attention == nullptr) {
        return;
    }

    // Co-op: lock cursors are view-owned in split screen, so do not enqueue them
    // into the shared 3D-last buffer that every camera replays.
    s_viewportCursorBuffer->frameInit();
    s_viewportCursorDrawActive = true;
    attention->Draw();
    s_viewportCursorDrawActive = false;
    s_viewportCursorBuffer->draw();
    s_viewportCursorBuffer->frameInit();
    dComIfGd_setList();
}

bool isActorLockedByAnyPlayer(const fopAc_ac_c* actor) {
    if (actor == nullptr) {
        return false;
    }

    dAttention_c* primaryAttention = dComIfGp_getAttention();
    if (primaryAttention->LockonTruth() && primaryAttention->LockonTarget(0) == actor) {
        return true;
    }

    for (int i = 1; i < kPlayerSlotCount; i++) {
        dAttention_c* attention = s_attention[i].attention;
        if (attention != nullptr && attention->LockonTruth() && attention->LockonTarget(0) == actor) {
            return true;
        }
    }

    return false;
}

fopAc_ac_c* lockingPlayerForActor(const fopAc_ac_c* actor) {
    if (actor == nullptr) {
        return nullptr;
    }

    dAttention_c* primaryAttention = dComIfGp_getAttention();
    if (primaryAttention->LockonTruth() && primaryAttention->LockonTarget(0) == actor) {
        return getPlayer(PlayerSlot::Primary);
    }

    for (int i = 1; i < kPlayerSlotCount; i++) {
        dAttention_c* attention = s_attention[i].attention;
        if (attention != nullptr && attention->LockonTruth() && attention->LockonTarget(0) == actor) {
            return getPlayer(static_cast<PlayerSlot>(i));
        }
    }

    return nullptr;
}

bool isLockBlockedByPlayerStatus(dAttention_c* attention) {
    if (attention == nullptr) {
        return true;
    }

    const PlayerSlot slot = getSlotForActor(attention->mpPlayer);
    if (slot == PlayerSlot::Invalid || slot == PlayerSlot::Primary) {
        return dComIfGp_checkPlayerStatus0(0, 0x36A02311) ||
               dComIfGp_checkPlayerStatus1(0, 0x11);
    }

    // Co-op: additional players must not borrow P1's singleton lock-out status. This leaves
    // P2 lock gating intentionally open until player_camera_status owns equivalent local bits.
    return false;
}

unsigned int attentionFlagsForOwner(dAttention_c* attention) {
    if (attention == nullptr || attention->mpPlayer == nullptr) {
        return 0;
    }

    const PlayerSlot slot = getSlotForActor(attention->mpPlayer);
    if (slot == PlayerSlot::Invalid || slot == PlayerSlot::Primary) {
        return attention->mpPlayer->attention_info.flags;
    }

    // Co-op: additional Link actors hide their actor attention flags so P1 cannot lock onto them,
    // but their own scanners still need the normal player capability mask to acquire targets.
    return 0xFFFFFFFF;
}

bool canSelectActor(dAttention_c* attention, const fopAc_ac_c* actor) {
    if (attention == nullptr || actor == nullptr) {
        return false;
    }

    const PlayerSlot ownerSlot = getSlotForActor(attention->mpPlayer);
    // Co-op: Sense-reveal actors stay in the shared attention list, but each scanner applies
    // the same owner-local reveal threshold that controls its viewport presentation.
    if (!player_sense::canReveal(ownerSlot, actor)) {
        return false;
    }
    if (ownerSlot == PlayerSlot::Invalid || ownerSlot == PlayerSlot::Primary) {
        return true;
    }

    // Co-op: P2 can target enemies/objects, but player actors are not lock-on targets.
    return getSlotForActor(actor) == PlayerSlot::Invalid;
}

unsigned int actorFlagsForOwner(dAttention_c* attention, const fopAc_ac_c* actor) {
    if (actor == nullptr) {
        return 0;
    }

    if (attention != nullptr && attention->mpPlayer != nullptr &&
        fpcM_GetName(actor) == fpcNm_Tag_Wljump_e)
    {
        // Co-op: one shared jump tag exposes the path point owned by each Link's native scanner.
        return static_cast<const daTagWljump_c*>(actor)->getAttentionFlags(
            static_cast<const daAlink_c*>(attention->mpPlayer));
    }

    return actor->attention_info.flags;
}

const cXyz& actorPositionForOwner(dAttention_c* attention, const fopAc_ac_c* actor) {
    if (attention != nullptr && attention->mpPlayer != nullptr && actor != nullptr &&
        fpcM_GetName(actor) == fpcNm_Tag_Wljump_e)
    {
        // Co-op: lock selection and cursor drawing must consume the same slot-local jump point.
        return static_cast<const daTagWljump_c*>(actor)->getAttentionPosition(
            static_cast<const daAlink_c*>(attention->mpPlayer));
    }

    return actor->attention_info.position;
}

bool isViewportCursorDrawActive() {
    return s_viewportCursorDrawActive;
}

void setViewportCursorDrawList() {
    if (!ensureViewportCursorBuffer()) {
        dComIfGd_setList3Dlast();
        return;
    }

    j3dSys.setDrawBuffer(s_viewportCursorBuffer, J3DSysDrawBuf_Opa);
    j3dSys.setDrawBuffer(s_viewportCursorBuffer, J3DSysDrawBuf_Xlu);
}

}  // namespace dusk::coop::player_attention
