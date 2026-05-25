#include "dusk/coop/render_shadows.h"

#include "dusk/coop/camera.h"
#include "dusk/coop/render_visibility.h"

namespace dusk::coop::render_shadows {

bool shouldBypassSharedShadowCulling() {
    return dusk::coop::render_visibility::shouldBypassDrawCulling();
}

bool shouldRefreshRealShadowForCurrentView() {
#if TARGET_PC
    return dusk::coop::camera::isSplitScreenEnabled();
#else
    return false;
#endif
}

}  // namespace dusk::coop::render_shadows
