#pragma once

#include "dolphin/types.h"

#include <cstdint>
#include <filesystem>

class cXyz;
class daAlink_c;
class fopAc_ac_c;

namespace dusk::diagnostics {

struct SecondaryAlinkState {
    const char* phase = nullptr;
    uintptr_t actor = 0;
    uintptr_t target = 0;
    uintptr_t anim = 0;
    uintptr_t modelUser = 0;
    uintptr_t ownerUnder = 0;
    uintptr_t ownerUpper = 0;
    uintptr_t itemActor = 0;
    uintptr_t rideActor = 0;
    uintptr_t throwBoomerangActor = 0;
    uintptr_t copyRodActor = 0;
    uintptr_t copyRodControlActor = 0;
    uintptr_t copyRodCameraActor = 0;
    uintptr_t wolfLockActor = 0;
    int itemActorId = 0;
    int rideActorId = 0;
    int wolfLockActorId = 0;
    s16 itemActorName = 0;
    s16 rideActorName = 0;
    s16 wolfLockActorName = 0;
    u16 proc = 0;
    u16 equipItem = 0;
    u8 selectItemId = 0;
    u8 rideStatus = 0;
    u8 activeBombCount = 0;
    u8 insectBombCount = 0;
    u8 itemButton = 0;
    u8 itemTrigger = 0;
    u8 useButtonFlags = 0;
    u8 previousUseButtonFlags = 0;
    u8 wolfLockNum = 0;
    s16 stickAngle = 0;
    s16 moveAngle = 0;
    s16 currentAngleY = 0;
    s16 shapeAngleY = 0;
    u32 attentionFlags = 0;
    u32 rawMask = 0;
    u8 rStatus = 0;
    f32 speedF = 0.0f;
    f32 normalSpeed = 0.0f;
    f32 stickValue = 0.0f;
    f32 moveValue = 0.0f;
    f32 posX = 0.0f;
    f32 posY = 0.0f;
    f32 posZ = 0.0f;
    f32 underFrame = 0.0f;
    f32 underRate = 0.0f;
    f32 wolfSearchBallScale = 0.0f;
    bool inputR = false;
    bool attentionLock = false;
    bool itemButtonR = false;
    bool itemTriggerR = false;
    bool copyRodTopUse = false;
    bool wolfLockChargeActive = false;
    bool wolfLockDomeActive = false;
    bool wolfLockAttackActive = false;
};

void setSecondaryAlinkActionMirrorProfileEnabled(bool enabled);
bool isSecondaryAlinkActionMirrorProfileEnabled();

void tick(u32 frame);
void flush(const char* reason);

void recordSecondaryAlinkState(const char* phase, const SecondaryAlinkState& state);
void recordCameraAreaLoadCheckpoint(const char* phase, const char* startupSource, int cameraId,
                                    const fopAc_ac_c* actor, const cXyz* center, const cXyz* eye,
                                    s16 cameraYaw, int startMode = -1, int cameraFrame = -1);
void recordMessageOwnerCheckpoint(const char* phase, int slot, int pad,
                                  const fopAc_ac_c* presenter, const fopAc_ac_c* listener,
                                  const fopAc_ac_c* speaker, bool fullscreenRequested,
                                  bool presentationActive, bool eventFullscreen,
                                  int presenterWindow, const char* source);
void recordTalkCameraCheckpoint(const char* phase, int cameraId, bool skipped,
                                const fopAc_ac_c* cameraPlayer,
                                const fopAc_ac_c* presenter, const fopAc_ac_c* listener,
                                const fopAc_ac_c* speaker, int talkCut, int eventAction,
                                const char* eventActionName, int cameraIsWolf = -1,
                                int presenterIsWolf = -1, int cameraStyle = -1,
                                int cameraType = -1, int cameraMode = -1,
                                int midnaRidingVisible = -1);
void recordTalkCameraViewCheckpoint(const char* phase, int cameraId, int talkCut,
                                    int talkTimer, int transitionTimer,
                                    const fopAc_ac_c* presenter,
                                    const fopAc_ac_c* listener,
                                    const fopAc_ac_c* speaker,
                                    const cXyz* viewCenter, const cXyz* viewEye,
                                    f32 viewRadius, int viewPitch, int viewYaw, f32 viewFovy,
                                    const cXyz* seededCenter, const cXyz* seededEye,
                                    f32 seededRadius, int seededPitch, int seededYaw,
                                    f32 seededFovy, const cXyz* listenerAim,
                                    const cXyz* speakerAim, const cXyz* listenerSpeakerDelta);
void recordWolfAoeCheckpoint(const char* phase, const daAlink_c* player, int cameraId,
                             u32 status0Mask, u32 status1Mask, f32 searchBallScale,
                             f32 cameraNearRadius, f32 cameraFarRadius, int wolfLockNum,
                             const fopAc_ac_c* lockActor, f32 cameraFovy = 0.0f,
                             f32 windowAspect = 0.0f, f32 windowWidth = 0.0f,
                             f32 windowHeight = 0.0f);

const std::filesystem::path& getOutputPath();

}  // namespace dusk::diagnostics
