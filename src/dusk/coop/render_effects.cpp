#include "dusk/coop/render_effects.h"

#include "dusk/coop/camera.h"

namespace dusk::coop::render_effects {

bool shouldReplayLateWorldEffectTail() {
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

}  // namespace dusk::coop::render_effects
