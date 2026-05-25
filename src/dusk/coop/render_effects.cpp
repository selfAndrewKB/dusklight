#include "dusk/coop/render_effects.h"

#include "dusk/coop/camera.h"

namespace dusk::coop::render_effects {

bool shouldReplayLateWorldEffectTail() {
    // Co-op: late world effects replay per viewport; fullscreen framebuffer effects are gated
    // inside that tail because they are still single-EFB ownership.
    return true;
}

bool shouldRunFullscreenFramebufferEffects() {
#if TARGET_PC
    return !dusk::coop::camera::isSplitScreenEnabled();
#else
    return true;
#endif
}

bool shouldRefreshProjectionParticleFramebuffer() {
#if TARGET_PC
    return dusk::coop::camera::isSplitScreenEnabled();
#else
    return false;
#endif
}

bool shouldBypassSharedParticleCreationCulling() {
#if TARGET_PC
    return dusk::coop::camera::isSplitScreenEnabled();
#else
    return false;
#endif
}

}  // namespace dusk::coop::render_effects
