#include "dusk/coop/render_shadows.h"

#include "dusk/coop/event_presentation.h"
#include "dusk/coop/render_visibility.h"

namespace dusk::coop::render_shadows {

bool shouldBypassSharedShadowCulling() {
    return dusk::coop::render_visibility::shouldBypassDrawCulling();
}

bool shouldRefreshRealShadowForCurrentView() {
#if TARGET_PC
    return dusk::coop::event_presentation::shouldRefreshViewportOwnedWorldState();
#else
    return false;
#endif
}

}  // namespace dusk::coop::render_shadows
