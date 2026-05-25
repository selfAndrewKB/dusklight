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

}  // namespace dusk::coop::render_effects
