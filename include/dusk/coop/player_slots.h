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

// Co-op: runtime switches for secondary ALINK audit probes.
enum SecondaryAlinkProbeFlag : unsigned int {
    SecondaryAlinkProbe_SkipExecute = 1u << 0,
    SecondaryAlinkProbe_SkipDraw = 1u << 1,
    SecondaryAlinkProbe_SkipWaitAnimeBind = 1u << 2,
    SecondaryAlinkProbe_SkipStartProcInit = 1u << 3,
    SecondaryAlinkProbe_SkipSetMatrix = 1u << 4,
    SecondaryAlinkProbe_SkipCreateAnimePlay = 1u << 5,
    SecondaryAlinkProbe_SkipCreateModelCalc = 1u << 6,
    SecondaryAlinkProbe_SkipFaceTextureAnime = 1u << 7,
    SecondaryAlinkProbe_SkipItemMatrix = 1u << 8,
    SecondaryAlinkProbe_SkipSetItemActor = 1u << 9,
    SecondaryAlinkProbe_RestorePrimaryModelDataOwner = 1u << 10,
    SecondaryAlinkProbe_ScopedDrawModelDataOwner = 1u << 11,
    SecondaryAlinkProbe_ScopedExecuteModelDataOwner = 1u << 12,
    SecondaryAlinkProbe_IgnoreSharedAttentionLock = 1u << 13,
};

constexpr unsigned int kDefaultSecondaryAlinkProbeFlags =
    SecondaryAlinkProbe_RestorePrimaryModelDataOwner |
    SecondaryAlinkProbe_ScopedDrawModelDataOwner |
    SecondaryAlinkProbe_ScopedExecuteModelDataOwner |
    SecondaryAlinkProbe_IgnoreSharedAttentionLock;

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

unsigned int getSecondaryAlinkProbeFlags();
void setSecondaryAlinkProbeFlags(unsigned int flags);
bool hasSecondaryAlinkProbeFlag(SecondaryAlinkProbeFlag flag);

}  // namespace dusk::coop
