#pragma once

#include "dusk/coop/player_slots.h"

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

struct DebugState {
    bool viewportActive = false;
    ViewportContext viewport = {};
    int bloomMode = 0;
    int bloomSourceWidth = 0;
    int bloomSourceHeight = 0;
    float bloomCompositeX = 0.0f;
    float bloomCompositeY = 0.0f;
    float bloomCompositeWidth = 0.0f;
    float bloomCompositeHeight = 0.0f;
    bool baseEnvironmentValid = false;
    bool senseEnvironmentValid = false;
    SenseDebugState sense[kPlayerSlotCount] = {};
    TwilightDebugState twilight[kPlayerSlotCount] = {};
};

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
void unregisterEmitter(const JPABaseEmitter* emitter);
bool shouldDrawEmitter(const JPABaseEmitter* emitter);

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
bool shouldBypassSharedParticleCreationCulling();

}  // namespace dusk::coop::render_effects
