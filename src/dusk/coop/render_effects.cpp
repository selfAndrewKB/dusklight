#include "dusk/coop/render_effects.h"

#include "dusk/coop/event_presentation.h"

namespace dusk::coop::render_effects {

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
    return dusk::coop::event_presentation::shouldPresentSplitViewports();
#else
    return false;
#endif
}

}  // namespace dusk::coop::render_effects
