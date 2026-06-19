#include "dusk/coop/render_visibility.h"

#include "dusk/coop/event_presentation.h"

namespace dusk::coop::render_visibility {

bool shouldBypassDrawCulling() {
#if TARGET_PC
    return dusk::coop::event_presentation::shouldRefreshViewportOwnedWorldState();
#else
    return false;
#endif
}

}  // namespace dusk::coop::render_visibility
