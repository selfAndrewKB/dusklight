#pragma once

#include "dusk/coop/player_slots.h"
#include <mtx.h>

class view_class;
class view_port_class;
class JPABaseEmitter;
struct BOSS_LIGHT;

namespace dusk::coop::render_effects {

struct ViewportContext {
    PlayerSlot slot = PlayerSlot::Primary;
    int windowIndex = 0;
    int cameraId = 0;
    view_class* view = nullptr;
    view_port_class* viewport = nullptr;
};

struct SenseDebugState {
    bool active = false;
    bool nowEffect = false;
    float strength = 0.0f;
    int emitterCount = 0;
};

struct TwilightDebugState {
    bool valid = false;
    int cameraId = 0;
    const void* player = nullptr;
    unsigned int activeMask = 0;
};

constexpr int kProjectionParticleDebugCapacity = 32;

struct ProjectionParticleDebugState {
    const void* emitter = nullptr;
    unsigned int resourceId = 0;
    int groupId = 0;
    int resourceManagerId = 0;
    int slot = 0;
    int windowIndex = 0;
    int cameraId = 0;
    int particleCount = 0;
    unsigned int status = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float cameraZ = 0.0f;
    bool hasFirstParticle = false;
    float firstParticleX = 0.0f;
    float firstParticleY = 0.0f;
    float firstParticleZ = 0.0f;
    float projectionScaleX = 0.0f;
    float projectionOffsetX = 0.0f;
    float projectionScaleY = 0.0f;
    float projectionOffsetY = 0.0f;
};

struct CloudHazeDebugState {
    bool simulationRecorded = false;
    bool drawRecorded = false;
    int mode = 0;
    int count = 0;
    const void* packet = nullptr;
    const void* sourceCamera = nullptr;
    const void* sourcePlayer = nullptr;
    int sourceCameraId = 0;
    float sourceEyeX = 0.0f;
    float sourceEyeY = 0.0f;
    float sourceEyeZ = 0.0f;
    float simulationCenterX = 0.0f;
    float simulationCenterY = 0.0f;
    float simulationCenterZ = 0.0f;
    int drawSlot = 0;
    int drawWindowIndex = 0;
    int drawCameraId = 0;
    float activeEyeX = 0.0f;
    float activeEyeY = 0.0f;
    float activeEyeZ = 0.0f;
    float activeFovy = 0.0f;
    float activeAspect = 0.0f;
    float projectionFovy = 0.0f;
    float projectionAspect = 0.0f;
    float firstCloudX = 0.0f;
    float firstCloudY = 0.0f;
    float firstCloudZ = 0.0f;
    int drawCalls[kPlayerSlotCount] = {};
    int drawCounts[kPlayerSlotCount] = {};
    int visibleCounts[kPlayerSlotCount] = {};
    float alphaSums[kPlayerSlotCount] = {};
};

struct DebugState {
    bool viewportActive = false;
    ViewportContext viewport = {};
    int bloomMode = 0;
    int bloomSourceWidth = 0;
    int bloomSourceHeight = 0;
    int bloomTargetWidth = 0;
    int bloomTargetHeight = 0;
    float bloomCompositeX = 0.0f;
    float bloomCompositeY = 0.0f;
    float bloomCompositeWidth = 0.0f;
    float bloomCompositeHeight = 0.0f;
    bool baseEnvironmentValid = false;
    bool senseEnvironmentValid = false;
    int senseRevealEmitterCount = 0;
    int senseInactiveEmitterCount = 0;
    SenseDebugState sense[kPlayerSlotCount] = {};
    TwilightDebugState twilight[kPlayerSlotCount] = {};
    int projectionParticleCount = 0;
    ProjectionParticleDebugState
        projectionParticles[kProjectionParticleDebugCapacity] = {};
    CloudHazeDebugState cloudHaze = {};
};

struct EmitterPresentationState {
    bool restoreAlpha = false;
    unsigned char alpha = 0;
    bool restoreCameraMatrix = false;
    Mtx cameraMatrix = {};
    void (*restoreCallback)(JPABaseEmitter*, void*) = nullptr;
    void* callbackContext = nullptr;
};

using EmitterViewportPrepareCallback =
    void (*)(JPABaseEmitter*, PlayerSlot, void*);
using EmitterViewportRestoreCallback = void (*)(JPABaseEmitter*, void*);

void beginViewport(int windowIndex, int cameraId, view_class* view, view_port_class* viewport);
void endViewport();
bool hasViewport();
const ViewportContext& currentViewport();
PlayerSlot currentViewportSlot();
int cameraIdForSlot(PlayerSlot slot);

void reset();

void updateSense();
bool isSenseActive(PlayerSlot slot);
bool isCurrentViewportSenseActive();
float senseStrength(PlayerSlot slot);
float currentViewportSenseStrength();
float senseStrengthForEmitter(const JPABaseEmitter* emitter);

void registerEmitterOwner(JPABaseEmitter* emitter, PlayerSlot slot);
void registerSenseRevealEmitter(JPABaseEmitter* emitter);
void registerSenseInactiveEmitter(JPABaseEmitter* emitter);
void registerCameraRelativeEmitter(JPABaseEmitter* emitter, int simulationCameraId,
                                   void* context,
                                   EmitterViewportPrepareCallback prepare,
                                   EmitterViewportRestoreCallback restore);
void unregisterEmitter(const JPABaseEmitter* emitter);
bool shouldDrawEmitter(const JPABaseEmitter* emitter);
EmitterPresentationState applyEmitterPresentation(JPABaseEmitter* emitter,
                                                  Mtx cameraMatrix);
void restoreEmitterPresentation(JPABaseEmitter* emitter,
                                const EmitterPresentationState& state,
                                Mtx cameraMatrix);

void captureEnvironmentVariant(bool sensed);
void applyEnvironmentForSlot(PlayerSlot slot);
void applyEnvironmentForCurrentViewport();

void storeTwilightLights(PlayerSlot slot, const BOSS_LIGHT* lights, int count, int cameraId,
                         const void* player, unsigned int activeMask);
void prepareTwilightLights(PlayerSlot slot, const BOSS_LIGHT* baseLights, BOSS_LIGHT* outLights,
                           int count);
void applyTwilightLightsForSlot(PlayerSlot slot);
void applyTwilightLightsForCurrentViewport();

void recordBloomPresentation(int mode, int sourceWidth, int sourceHeight, float compositeX,
                             float compositeY, float compositeWidth, float compositeHeight);
void recordBloomTarget(int targetWidth, int targetHeight);
void recordProjectionParticle(const JPABaseEmitter* emitter, int groupId,
                              const Mtx cameraMatrix, const Mtx projectionMatrix);
void updateCloudHazeSimulation(int mode, int count, const void* packet,
                               const void* sourceCamera, const void* sourcePlayer,
                               int sourceCameraId, float sourceEyeX, float sourceEyeY,
                               float sourceEyeZ, float centerX, float centerY, float centerZ);
void recordCloudHazeDraw(int mode, int count, const void* packet, int sourceCameraId,
                         float activeEyeX, float activeEyeY, float activeEyeZ,
                         float activeFovy, float activeAspect, float projectionFovy,
                         float projectionAspect, float firstCloudX, float firstCloudY,
                         float firstCloudZ, int visibleCount, float alphaSum);
void drawViewportSafeIndirectWorldEffects();
DebugState getDebugState();

bool shouldReplayLateWorldEffectTail();
bool shouldRunFullscreenFramebufferEffects();
bool shouldRunViewportBloom();
bool shouldRunMotionBlur();
bool shouldRunDepthOfField();
bool shouldRunIndirectScreenPasses();
bool shouldRunFullscreen2DOverlays();
bool shouldRunFades();
bool shouldDrawViewportTrim();
bool shouldRefreshInvisibleListFramebuffer();
bool shouldRefreshProjectionParticleFramebuffer();
bool shouldRefreshScreenParticleFramebuffer();
bool shouldBypassSharedParticleCreationCulling();

}  // namespace dusk::coop::render_effects
