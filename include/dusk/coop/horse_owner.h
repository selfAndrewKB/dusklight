#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"
#include "f_pc/f_pc_manager.h"

#include <cstdint>

class daAlink_c;
class daHorse_c;
class fopAc_ac_c;
class J3DAnmTransform;

namespace dusk::coop::horse_owner {

constexpr int kFirstAdditionalHorseSpawnArgument = -2;
constexpr int kHorseReinSimulationMaxPoints = 75;

enum class HorseSummonDecision : u8 {
    None,
    NoPlayer,
    DeferredPlayer,
    DeferredSpawn,
    AppliedDeferred,
    CalledParked,
    CalledPresented,
    SkippedRidden,
    InvalidState,
};

struct HorseReinSimulationState {
    cXyz previous[kHorseReinSimulationMaxPoints];
    cXyz current[kHorseReinSimulationMaxPoints];
    int previousCount = 0;
    int currentCount = 0;
    bool previousValid = false;
    bool currentValid = false;
    uint64_t rolledSequence = 0;
};

bool isAdditionalHorseSpawnRequest(const fopAc_ac_c* actor);
PlayerSlot getAdditionalHorseSpawnRequestSlot(const fopAc_ac_c* actor);

void registerHorse(PlayerSlot slot, daHorse_c* horse);
void unregisterHorse(PlayerSlot slot, const daHorse_c* horse);

daHorse_c* getHorse(PlayerSlot slot);
daHorse_c* getHorseForPlayer(const daAlink_c* player);
daAlink_c* getPlayerForHorse(const daHorse_c* horse);
PlayerSlot getSlotForHorse(const daHorse_c* horse);
bool isCanonicalHorse(const daHorse_c* horse);
bool isAdditionalHorse(const daHorse_c* horse);
bool shouldPresentLashMeter(PlayerSlot slot);
bool anyHorseNeedsLashMeter();

template <typename Func>
void forEachRegisteredHorse(Func fn) {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        PlayerSlot slot = static_cast<PlayerSlot>(i);
        daHorse_c* horse = getHorse(slot);
        if (horse != nullptr) {
            fn(slot, horse);
        }
    }
}

void ensureHorseForSlot(PlayerSlot slot);
void ensureAdditionalHorses();
void callParkedAdditionalHorsesForCanonicalSummon(const cXyz& summonPos);
void presentParkedAdditionalHorsesForCanonicalPlacement(const cXyz& pos, s16 angle);
void releaseHorseForSlot(PlayerSlot slot);

void setCallTarget(daHorse_c* horse, const cXyz& pos);
void clearCallTarget(daHorse_c* horse);
const cXyz* getCallTarget(const daHorse_c* horse);
const cXyz* getCallTarget(PlayerSlot slot);
void updateDeferredSummonForHorse(daHorse_c* horse);
void setNextSummonActivator(PlayerSlot slot);
bool isCallDeferred(PlayerSlot slot);
f32 getCallDelaySeconds(PlayerSlot slot);
bool isPlacementDeferred(PlayerSlot slot);
u32 getLastSummonRevision();
PlayerSlot getLastSummonActivator();
const cXyz* getLastSummonPos();
HorseSummonDecision getLastSummonDecision(PlayerSlot slot);
const char* getHorseSummonDecisionName(HorseSummonDecision decision);

fpc_ProcID getPendingHorseSpawnId(PlayerSlot slot);
J3DAnmTransform* localizeAnimationTransform(daHorse_c* horse, J3DAnmTransform* animation);
int getLocalizedAnimationCount(const daHorse_c* horse);
HorseReinSimulationState* getReinSimulationState(daHorse_c* horse);
const HorseReinSimulationState* getReinSimulationState(const daHorse_c* horse);
void copyReinSimulationState(daHorse_c* horse, const cXyz* points, int count);
void lerpRegisteredHorseReins(f32 alpha);

}  // namespace dusk::coop::horse_owner
