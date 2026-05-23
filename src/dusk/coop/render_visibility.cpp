#include "dusk/coop/render_visibility.h"

#include "dusk/coop/camera.h"

namespace dusk::coop::render_visibility {

bool shouldBypassDrawCulling() {
#if TARGET_PC
    return dusk::coop::camera::isSplitScreenEnabled();
#else
    return false;
#endif
}

}  // namespace dusk::coop::render_visibility
