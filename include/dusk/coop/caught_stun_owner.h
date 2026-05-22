#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class daPy_py_c;
class fopAc_ac_c;

namespace dusk::coop::caught_stun_owner {

enum class CaughtStunOwnerReason : u8 {
    DirectPlayer,
    FallbackPrimary,
    Cleared,
};

struct CaughtStunActorDebug {
    uintptr_t ptr = 0;
    int profile = 0;
    int name = 0;
    int id = 0;
    int room = 0;
    int argument = 0;
    f32 pos[3] = {};
    s16 angleY = 0;
    bool available = false;
};

struct CaughtStunOwnerState {
    fopAc_ac_c* enemy = nullptr;
    PlayerSlot slot = PlayerSlot::Invalid;
    daPy_py_c* localPlayer = nullptr;
    fopAc_ac_c* localPlayerActor = nullptr;
    PlayerSlot affectedSlots[kPlayerSlotCount] = {};
    fopAc_ac_c* affectedLocalActors[kPlayerSlotCount] = {};
    CaughtStunActorDebug enemyDebug;
    CaughtStunActorDebug playerDebug;
    CaughtStunOwnerReason reason = CaughtStunOwnerReason::Cleared;
    int affectedCount = 0;
    int stunTimer = 0;
    int cryTimer = 0;
    bool active = false;
    bool found = false;
};

struct CaughtStunOwnerDecisionDebug {
    u64 eventId = 0;
    u32 simFrame = 0;
    char label[64] = {};
    CaughtStunOwnerState state;
};

struct CaughtStunOwnerDebugState {
    CaughtStunOwnerDecisionDebug decisions[32] = {};
    int decisionCount = 0;
    u32 currentSimFrame = 0;
};

void advanceCaughtStunOwnerFrame(u32 frame);
CaughtStunOwnerState beginCaughtStun(const char* label, fopAc_ac_c* enemy,
                                     daPy_py_c* player);
CaughtStunOwnerState beginCaughtStunArea(const char* label, fopAc_ac_c* enemy,
                                         daPy_py_c* primaryPlayer, f32 rangeXZ);
CaughtStunOwnerState updateCaughtStun(const char* label, fopAc_ac_c* enemy,
                                      int stunTimer, int cryTimer);
void clearCaughtStun(const char* label, fopAc_ac_c* enemy);
CaughtStunOwnerState getCaughtStun(fopAc_ac_c* enemy);

const CaughtStunOwnerDebugState& getCaughtStunOwnerDebugState();
const char* caughtStunOwnerReasonName(CaughtStunOwnerReason reason);

}  // namespace dusk::coop::caught_stun_owner
