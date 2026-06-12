#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class daAlink_c;
class daMidna_c;
class fopAc_ac_c;

namespace dusk::coop::midna_owner {

constexpr int kFirstAdditionalMidnaSpawnArgument = -2;

bool isAdditionalMidnaSpawnRequest(const fopAc_ac_c* actor);
PlayerSlot getAdditionalMidnaSpawnRequestSlot(const fopAc_ac_c* actor);
void registerMidna(PlayerSlot slot, daMidna_c* midna);
void unregisterMidna(PlayerSlot slot, const daMidna_c* midna);
daMidna_c* getMidna(PlayerSlot slot);
daMidna_c* getMidnaForPlayer(const daAlink_c* player);
daAlink_c* getPlayerForMidna(const daMidna_c* midna);
PlayerSlot getSlotForMidna(const daMidna_c* midna);
void ensureMidnaForSlot(PlayerSlot slot);
void ensureAdditionalMidnas();
void releaseMidnaForSlot(PlayerSlot slot);

bool canUseService(const daAlink_c* player);
void beginService(daAlink_c* player, fopAc_ac_c* partner);
void requestEndService();
void endService();
void finishPendingEndService();
void updateService();
void reset();

bool isServiceActive();
bool retainsTalkCamera();
void markTalkCameraSeed(daMidna_c* midna, bool startupUnstable);
bool shouldReseedTalkCameraForStableMidna(daMidna_c* midna, bool poseReady);
void markTalkCameraStableReseeded();
PlayerSlot currentSlot();
daAlink_c* currentPlayer();
daAlink_c* messageFlowPlayer();
fopAc_ac_c* talkPartnerForPlayer(const daAlink_c* player);

bool isServicePartner(const fopAc_ac_c* partner);
bool shouldConsumeAlinkStaff(const daAlink_c* player);
bool shouldSkipAlinkStaff(const daAlink_c* player);
s32 orderPotentialEvent(fopAc_ac_c* requester, u16 flags, u16 hindFlags, u16 priority);

bool currentPlayerIsWolf();
bool currentPlayerRidesHorseOrBoar();
int transformBlockReasonForPlayer(const daAlink_c* player);
bool canTransformNow(const daAlink_c* player);
int currentTransformBlockReason();
u16 currentMidnaMsgNum();
void markCurrentMidnaMsgUsed();

bool checkTalkStatus(const daAlink_c* player);
void setTalkStatus(daAlink_c* player);
void clearTalkStatus(daAlink_c* player);

}  // namespace dusk::coop::midna_owner
