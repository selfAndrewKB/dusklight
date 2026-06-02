#pragma once

class fopAc_ac_c;
class daAlink_c;

namespace dusk::coop {

// Co-op: sidecar identity for player actors without changing vanilla game structs.
enum class PlayerSlot : unsigned char {
    Slot0 = 0,
    Slot1 = 1,
    Slot2 = 2,
    Slot3 = 3,
    Primary = Slot0,
    Secondary = Slot1,
    Invalid = 0xff,
};

constexpr int kPlayerSlotCount = 4;
constexpr int kFirstAdditionalPlayerSpawnArgument = -2;

void registerPlayer(PlayerSlot slot, fopAc_ac_c* actor);
void unregisterPlayer(PlayerSlot slot, const fopAc_ac_c* actor);

fopAc_ac_c* getPlayer(PlayerSlot slot);
fopAc_ac_c* getPrimaryPlayer();

bool isPrimaryPlayer(const fopAc_ac_c* actor);
bool isPlayerInSlot(const fopAc_ac_c* actor, PlayerSlot slot);
bool isSecondaryPlayer(const fopAc_ac_c* actor);
bool isAdditionalPlayer(const fopAc_ac_c* actor);
bool isAdditionalPlayerSpawnRequest(const fopAc_ac_c* actor);
PlayerSlot getAdditionalPlayerSpawnRequestSlot(const fopAc_ac_c* actor);
PlayerSlot getSlotForActor(const fopAc_ac_c* actor);
int getPadForSlot(PlayerSlot slot);
unsigned int spawnPlayer(PlayerSlot slot, daAlink_c* primary);
void restoreRequestedPlayers(daAlink_c* primary);
bool isPlayerRequested(PlayerSlot slot);

}  // namespace dusk::coop
