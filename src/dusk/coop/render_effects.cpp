#include "dusk/coop/render_effects.h"

#include "JSystem/JParticle/JPAEmitter.h"
#include "SSystem/SComponent/c_lib.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "dusk/coop/camera.h"
#include "dusk/coop/event_presentation.h"
#include "m_Do/m_Do_graphic.h"

namespace dusk::coop::render_effects {
namespace {

constexpr int kViewportStackCapacity = 4;
constexpr int kEmitterOwnerCapacity = 32;
constexpr int kTwilightLightCount = 8;
ViewportContext s_viewportStack[kViewportStackCapacity] = {};
int s_viewportDepth = 0;

struct SenseState {
    bool active = false;
    bool nowEffect = false;
    unsigned char mode = 0;
    float strength = 0.0f;
    JPABaseEmitter* emitters[3] = {};
};

struct EmitterOwner {
    const JPABaseEmitter* emitter = nullptr;
    PlayerSlot slot = PlayerSlot::Invalid;
};

struct BloomSnapshot {
    GXColor blendColor = {};
    GXColor monoColor = {};
    unsigned char enable = 0;
    unsigned char mode = 0;
    unsigned char point = 0;
    unsigned char blurSize = 0;
    unsigned char blurRatio = 0;
};

struct EnvironmentSnapshot {
    bool valid = false;
    GXColorS10 skyColor = {};
    GXColorS10 cloudTopColor = {};
    GXColorS10 cloudBottomColor = {};
    GXColorS10 cloudShadowColor = {};
    GXColorS10 hazeOuterColor = {};
    GXColorS10 hazeInnerColor = {};
    GXColorS10 actorAmbient = {};
    GXColorS10 backgroundAmbient[4] = {};
    GXColorS10 dungeonLightColor[6] = {};
    GXColorS10 fogColor = {};
    float fogNear = 0.0f;
    float fogFar = 0.0f;
    unsigned char fogDensity = 0;
    BloomSnapshot bloom = {};
};

struct TwilightSnapshot {
    bool valid = false;
    BOSS_LIGHT lights[kTwilightLightCount] = {};
    int cameraId = 0;
    const void* player = nullptr;
    unsigned int activeMask = 0;
};

SenseState s_senseStates[kPlayerSlotCount] = {};
EmitterOwner s_emitterOwners[kEmitterOwnerCapacity] = {};
EnvironmentSnapshot s_environment[2] = {};
TwilightSnapshot s_twilight[kPlayerSlotCount] = {};
ViewportContext s_lastViewport = {};
int s_bloomMode = 0;
int s_bloomSourceWidth = 0;
int s_bloomSourceHeight = 0;
float s_bloomCompositeX = 0.0f;
float s_bloomCompositeY = 0.0f;
float s_bloomCompositeWidth = 0.0f;
float s_bloomCompositeHeight = 0.0f;

PlayerSlot slotForCamera(int cameraId) {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        PlayerSlot slot = static_cast<PlayerSlot>(i);
        if (getPlayer(slot) != nullptr &&
            (i == 0 || dusk::coop::camera::isExtensionIndex(i)) &&
            cameraIdForSlot(slot) == cameraId)
        {
            return slot;
        }
    }

    return PlayerSlot::Primary;
}

int slotIndex(PlayerSlot slot) {
    const int index = static_cast<int>(slot);
    return index >= 0 && index < kPlayerSlotCount ? index : 0;
}

SenseState& senseState(PlayerSlot slot) {
    return s_senseStates[slotIndex(slot)];
}

PlayerSlot emitterOwner(const JPABaseEmitter* emitter) {
    for (int i = 0; i < kEmitterOwnerCapacity; i++) {
        if (s_emitterOwners[i].emitter == emitter) {
            return s_emitterOwners[i].slot;
        }
    }

    return PlayerSlot::Invalid;
}

void stopSenseEmitter(JPABaseEmitter*& emitter) {
    if (emitter == nullptr) {
        return;
    }

    emitter->deleteAllParticle();
    emitter->becomeInvalidEmitter();
    emitter->quitImmortalEmitter();
    emitter->setEmitterCallBackPtr(nullptr);
    emitter = nullptr;
}

BloomSnapshot captureBloom() {
    mDoGph_gInf_c::bloom_c* bloom = mDoGph_gInf_c::getBloom();
    return {
        *bloom->getBlendColor(), *bloom->getMonoColor(), bloom->mEnable, bloom->mMode,
        bloom->getPoint(), bloom->getBlureSize(), bloom->getBlureRatio(),
    };
}

void applyBloom(const BloomSnapshot& state) {
    mDoGph_gInf_c::bloom_c* bloom = mDoGph_gInf_c::getBloom();
    bloom->setBlendColor(state.blendColor);
    bloom->setMonoColor(state.monoColor);
    bloom->setEnable(state.enable);
    bloom->setMode(state.mode);
    bloom->setPoint(state.point);
    bloom->setBlureSize(state.blurSize);
    bloom->setBlureRatio(state.blurRatio);
}

}  // namespace

void beginViewport(int windowIndex, int cameraId, view_class* view, view_port_class* viewport) {
#if TARGET_PC
    if (s_viewportDepth < kViewportStackCapacity) {
        s_viewportStack[s_viewportDepth++] = {
            slotForCamera(cameraId), windowIndex, cameraId, view, viewport,
        };
        s_lastViewport = s_viewportStack[s_viewportDepth - 1];
    }
#endif
}

void endViewport() {
#if TARGET_PC
    if (s_viewportDepth > 0) {
        s_viewportDepth--;
    }
#endif
}

bool hasViewport() {
#if TARGET_PC
    return s_viewportDepth > 0;
#else
    return false;
#endif
}

const ViewportContext& currentViewport() {
    static const ViewportContext primary = {};
#if TARGET_PC
    if (s_viewportDepth > 0) {
        return s_viewportStack[s_viewportDepth - 1];
    }
#endif
    return primary;
}

PlayerSlot currentViewportSlot() {
    return currentViewport().slot;
}

int cameraIdForSlot(PlayerSlot slot) {
    const int index = slotIndex(slot);
    // Co-op: only query vanilla or installed camera-sidecar entries; future slots use Camera 0
    // until they gain a render window rather than indexing past vanilla player-camera storage.
    if (index == 0 || dusk::coop::camera::isExtensionIndex(index)) {
        return dComIfGp_getPlayerCameraID(index);
    }
    return dComIfGp_getPlayerCameraID(0);
}

void reset() {
    s_viewportDepth = 0;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        s_senseStates[i] = {};
        s_twilight[i] = {};
    }
    for (int i = 0; i < kEmitterOwnerCapacity; i++) {
        s_emitterOwners[i] = {};
    }
    s_environment[0] = {};
    s_environment[1] = {};
    s_lastViewport = {};
    s_bloomMode = 0;
    s_bloomSourceWidth = 0;
    s_bloomSourceHeight = 0;
    s_bloomCompositeX = 0.0f;
    s_bloomCompositeY = 0.0f;
    s_bloomCompositeWidth = 0.0f;
    s_bloomCompositeHeight = 0.0f;
}

void registerEmitterOwner(JPABaseEmitter* emitter, PlayerSlot slot) {
    if (emitter == nullptr) {
        return;
    }

    for (int i = 0; i < kEmitterOwnerCapacity; i++) {
        if (s_emitterOwners[i].emitter == emitter || s_emitterOwners[i].emitter == nullptr) {
            s_emitterOwners[i] = {emitter, slot};
            return;
        }
    }
}

void unregisterEmitter(const JPABaseEmitter* emitter) {
    for (int i = 0; i < kEmitterOwnerCapacity; i++) {
        if (s_emitterOwners[i].emitter == emitter) {
            s_emitterOwners[i] = {};
            return;
        }
    }
}

bool shouldDrawEmitter(const JPABaseEmitter* emitter) {
    const PlayerSlot owner = emitterOwner(emitter);
    return owner == PlayerSlot::Invalid || !hasViewport() || owner == currentViewportSlot();
}

bool isSenseActive(PlayerSlot slot) {
    daAlink_c* player = static_cast<daAlink_c*>(getPlayer(slot));
    return player != nullptr && player->checkWolfEyeUp() != 0;
}

bool isCurrentViewportSenseActive() {
    return isSenseActive(currentViewportSlot());
}

float senseStrength(PlayerSlot slot) {
    return senseState(slot).strength;
}

float currentViewportSenseStrength() {
    return senseStrength(currentViewportSlot());
}

float senseStrengthForEmitter(const JPABaseEmitter* emitter) {
    const PlayerSlot owner = emitterOwner(emitter);
    return owner == PlayerSlot::Invalid ? g_env_light.senses_effect_strength
                                        : senseStrength(owner);
}

void updateSense() {
    const cXyz position(0.0f, 0.0f, 0.0f);
    cXyz scale(1.0f, 1.0f, 1.0f);
    scale.x *= mDoGph_gInf_c::getScale();

    for (int i = 0; i < kPlayerSlotCount; i++) {
        const PlayerSlot slot = static_cast<PlayerSlot>(i);
        SenseState& state = s_senseStates[i];
        const bool active = isSenseActive(slot);
        bool transitioning = false;

        if (active) {
            state.nowEffect = true;
            transitioning = true;
            if (state.strength <= 0.0f) {
                JPABaseEmitter* emitter = dComIfGp_particle_set(0x1E3, &position, nullptr, nullptr,
                                                                &scale);
                registerEmitterOwner(emitter, slot);
            }
            cLib_addCalc(&state.strength, 1.0f, 0.5f, 0.1f, 0.01f);
        } else if (state.strength > 0.0f) {
            if (state.strength >= 1.0f) {
                JPABaseEmitter* emitter = dComIfGp_particle_set(0x46A, &position, nullptr, nullptr,
                                                                &scale);
                registerEmitterOwner(emitter, slot);
            }
            cLib_addCalc(&state.strength, 0.0f, 0.5f, 0.1f, 0.01f);
            transitioning = true;
        }

        state.active = active;
        switch (state.mode) {
        case 0:
            if (!transitioning) {
                break;
            }
            state.emitters[0] = dComIfGp_particle_set(0x1F2, &position, nullptr, nullptr);
            state.emitters[1] = dComIfGp_particle_set(0x1F3, &position, nullptr, nullptr, &scale);
            state.emitters[2] = dComIfGp_particle_set(0x1F4, &position, nullptr, nullptr, &scale);
            for (int j = 0; j < 3; j++) {
                registerEmitterOwner(state.emitters[j], slot);
            }
            state.mode++;
        case 1:
            if (transitioning) {
                if (state.emitters[0] != nullptr) {
                    state.emitters[0]->setGlobalTranslation(position.x, position.y, position.z);
                    state.emitters[0]->setGlobalAlpha(state.strength * 255.0f);
                }
                if (state.emitters[1] != nullptr) {
                    state.emitters[1]->setGlobalParticleScale(mDoGph_gInf_c::getScale(), 1.0f);
                    state.emitters[1]->setGlobalTranslation(position.x, position.y, position.z);
                    state.emitters[1]->setGlobalAlpha(state.strength * 255.0f);
                }
                if (state.emitters[2] != nullptr) {
                    state.emitters[2]->setGlobalParticleScale(mDoGph_gInf_c::getScale(), 1.0f);
                    state.emitters[2]->setGlobalTranslation(position.x, position.y, position.z);
                    const unsigned char color = 255.0f * (1.0f - state.strength);
                    state.emitters[2]->setGlobalEnvColor(color, color, color);
                }
            } else {
                state.mode++;
            }
            break;
        case 2:
            for (int j = 0; j < 3; j++) {
                stopSenseEmitter(state.emitters[j]);
            }
            state.mode = 0;
            break;
        }
    }

    const SenseState& primary = senseState(PlayerSlot::Primary);
    g_env_light.senses_mode = primary.mode;
    g_env_light.now_senses_effect = primary.nowEffect;
    g_env_light.senses_effect_strength = primary.strength;
    g_env_light.senses_ef_emitter0 = primary.emitters[0];
    g_env_light.senses_ef_emitter1 = primary.emitters[1];
    g_env_light.senses_ef_emitter2 = primary.emitters[2];
}

void captureEnvironmentVariant(bool sensed) {
    EnvironmentSnapshot& state = s_environment[sensed ? 1 : 0];
    state.valid = true;
    state.skyColor = g_env_light.vrbox_sky_col;
    state.cloudTopColor = g_env_light.vrbox_kumo_top_col;
    state.cloudBottomColor = g_env_light.vrbox_kumo_bottom_col;
    state.cloudShadowColor = g_env_light.vrbox_kumo_shadow_col;
    state.hazeOuterColor = g_env_light.vrbox_kasumi_outer_col;
    state.hazeInnerColor = g_env_light.vrbox_kasumi_inner_col;
    state.actorAmbient = g_env_light.actor_amb_col;
    for (int i = 0; i < 4; i++) {
        state.backgroundAmbient[i] = g_env_light.bg_amb_col[i];
    }
    for (int i = 0; i < 6; i++) {
        state.dungeonLightColor[i] = g_env_light.dungeonlight_col[i];
    }
    state.fogColor = g_env_light.fog_col;
    state.fogNear = g_env_light.mFogNear;
    state.fogFar = g_env_light.mFogFar;
    state.fogDensity = g_env_light.mFogDensity;
    state.bloom = captureBloom();
}

void applyEnvironmentForSlot(PlayerSlot slot) {
    const EnvironmentSnapshot& state = s_environment[isSenseActive(slot) ? 1 : 0];
    if (!state.valid) {
        return;
    }

    g_env_light.vrbox_sky_col = state.skyColor;
    g_env_light.vrbox_kumo_top_col = state.cloudTopColor;
    g_env_light.vrbox_kumo_bottom_col = state.cloudBottomColor;
    g_env_light.vrbox_kumo_shadow_col = state.cloudShadowColor;
    g_env_light.vrbox_kasumi_outer_col = state.hazeOuterColor;
    g_env_light.vrbox_kasumi_inner_col = state.hazeInnerColor;
    g_env_light.actor_amb_col = state.actorAmbient;
    for (int i = 0; i < 4; i++) {
        g_env_light.bg_amb_col[i] = state.backgroundAmbient[i];
    }
    for (int i = 0; i < 6; i++) {
        g_env_light.dungeonlight_col[i] = state.dungeonLightColor[i];
        g_env_light.dungeonlight[i].mColor.r = state.dungeonLightColor[i].r;
        g_env_light.dungeonlight[i].mColor.g = state.dungeonLightColor[i].g;
        g_env_light.dungeonlight[i].mColor.b = state.dungeonLightColor[i].b;
    }
    g_env_light.fog_col = state.fogColor;
    g_env_light.mFogNear = state.fogNear;
    g_env_light.mFogFar = state.fogFar;
    g_env_light.mFogDensity = state.fogDensity;
    applyBloom(state.bloom);
}

void applyEnvironmentForCurrentViewport() {
    applyEnvironmentForSlot(currentViewportSlot());
}

void storeTwilightLights(PlayerSlot slot, const BOSS_LIGHT* lights, int count, int cameraId,
                         const void* player, unsigned int activeMask) {
    TwilightSnapshot& state = s_twilight[slotIndex(slot)];
    state = {};
    state.valid = lights != nullptr;
    state.cameraId = cameraId;
    state.player = player;
    state.activeMask = activeMask;
    for (int i = 0; i < count && i < kTwilightLightCount; i++) {
        state.lights[i] = lights[i];
    }
}

void prepareTwilightLights(PlayerSlot slot, const BOSS_LIGHT* baseLights, BOSS_LIGHT* outLights,
                           int count) {
    const TwilightSnapshot& previous = s_twilight[slotIndex(slot)];
    for (int i = 0; i < count && i < kTwilightLightCount; i++) {
        outLights[i] = baseLights[i];
        if (previous.valid && baseLights[i].field_0x26 != 1) {
            outLights[i].mRefDistance = previous.lights[i].mRefDistance;
        }
    }
}

void applyTwilightLightsForSlot(PlayerSlot slot) {
    const TwilightSnapshot& state = s_twilight[slotIndex(slot)];
    if (!state.valid) {
        return;
    }
    for (int i = 0; i < kTwilightLightCount; i++) {
        g_env_light.field_0x0c18[i] = state.lights[i];
    }
}

void applyTwilightLightsForCurrentViewport() {
    applyTwilightLightsForSlot(currentViewportSlot());
}

void recordBloomPresentation(int mode, int sourceWidth, int sourceHeight, float compositeX,
                             float compositeY, float compositeWidth, float compositeHeight) {
    s_bloomMode = mode;
    s_bloomSourceWidth = sourceWidth;
    s_bloomSourceHeight = sourceHeight;
    s_bloomCompositeX = compositeX;
    s_bloomCompositeY = compositeY;
    s_bloomCompositeWidth = compositeWidth;
    s_bloomCompositeHeight = compositeHeight;
}

DebugState getDebugState() {
    DebugState state = {};
    state.viewportActive = hasViewport();
    state.viewport = hasViewport() ? currentViewport() : s_lastViewport;
    state.bloomMode = s_bloomMode;
    state.bloomSourceWidth = s_bloomSourceWidth;
    state.bloomSourceHeight = s_bloomSourceHeight;
    state.bloomCompositeX = s_bloomCompositeX;
    state.bloomCompositeY = s_bloomCompositeY;
    state.bloomCompositeWidth = s_bloomCompositeWidth;
    state.bloomCompositeHeight = s_bloomCompositeHeight;
    state.baseEnvironmentValid = s_environment[0].valid;
    state.senseEnvironmentValid = s_environment[1].valid;
    for (int i = 0; i < kPlayerSlotCount; i++) {
        const SenseState& sense = s_senseStates[i];
        state.sense[i].active = sense.active;
        state.sense[i].nowEffect = sense.nowEffect;
        state.sense[i].strength = sense.strength;
        for (int j = 0; j < kEmitterOwnerCapacity; j++) {
            if (s_emitterOwners[j].emitter != nullptr &&
                s_emitterOwners[j].slot == static_cast<PlayerSlot>(i))
            {
                state.sense[i].emitterCount++;
            }
        }
        state.twilight[i].valid = s_twilight[i].valid;
        state.twilight[i].cameraId = s_twilight[i].cameraId;
        state.twilight[i].player = s_twilight[i].player;
        state.twilight[i].activeMask = s_twilight[i].activeMask;
    }
    return state;
}

bool shouldReplayLateWorldEffectTail() {
    // Co-op: late world effects replay per viewport; fullscreen framebuffer effects are gated
    // inside that tail because they are still single-EFB ownership.
    return true;
}

bool shouldRunFullscreenFramebufferEffects() {
#if TARGET_PC
    return !dusk::coop::event_presentation::shouldPresentSplitViewports();
#else
    return true;
#endif
}

bool shouldDrawViewportTrim() {
    // Co-op: trim bars are viewport-local camera presentation, not a shared
    // framebuffer filter, so split-screen can draw them after fullscreen effects are gated.
    return true;
}

bool shouldRefreshInvisibleListFramebuffer() {
#if TARGET_PC
    return dusk::coop::event_presentation::shouldPresentSplitViewports();
#else
    return false;
#endif
}

bool shouldRefreshProjectionParticleFramebuffer() {
#if TARGET_PC
    return dusk::coop::event_presentation::shouldPresentSplitViewports();
#else
    return false;
#endif
}

bool shouldBypassSharedParticleCreationCulling() {
#if TARGET_PC
    return dusk::coop::event_presentation::shouldRefreshViewportOwnedWorldState();
#else
    return false;
#endif
}

bool shouldRunViewportBloom() {
    // Co-op: bloom now captures, filters, and composites only the active render viewport.
    return true;
}

bool shouldRunMotionBlur() {
    return shouldRunFullscreenFramebufferEffects();
}

bool shouldRunDepthOfField() {
    return shouldRunFullscreenFramebufferEffects();
}

bool shouldRunIndirectScreenPasses() {
    return shouldRunFullscreenFramebufferEffects();
}

bool shouldRunFullscreen2DOverlays() {
    return shouldRunFullscreenFramebufferEffects();
}

bool shouldRunFades() {
    return shouldRunFullscreenFramebufferEffects();
}

}  // namespace dusk::coop::render_effects
