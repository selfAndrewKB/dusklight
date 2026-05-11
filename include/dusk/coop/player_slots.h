#pragma once

class fopAc_ac_c;

namespace dusk::coop {

// Co-op: sidecar identity for player actors without changing vanilla game structs.
enum class PlayerSlot : unsigned char {
    Primary = 0,
    Secondary = 1,
    Invalid = 0xff,
};

constexpr int kPlayerSlotCount = 2;

void registerPlayer(PlayerSlot slot, fopAc_ac_c* actor);
void unregisterPlayer(PlayerSlot slot, const fopAc_ac_c* actor);

fopAc_ac_c* getPlayer(PlayerSlot slot);
fopAc_ac_c* getPrimaryPlayer();

bool isPrimaryPlayer(const fopAc_ac_c* actor);
PlayerSlot getSlotForActor(const fopAc_ac_c* actor);
int getPadForSlot(PlayerSlot slot);

}  // namespace dusk::coop
