#include "dusk/coop/ui_owner.h"

#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "d/d_com_inf_game.h"
#include "d/d_camera.h"
#include "d/d_drawlist.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"

namespace dusk::coop::ui_owner {
namespace {

PlayerSlot s_presentationStack[kPlayerSlotCount] = {};
int s_presentationDepth = 0;
PlayerSlot s_singularSlot = PlayerSlot::Primary;

PlayerSlot normalizeSlot(PlayerSlot slot) {
    if (slot == PlayerSlot::Invalid || static_cast<unsigned int>(slot) >= kPlayerSlotCount) {
        return PlayerSlot::Primary;
    }
    return slot;
}

int cameraIdForSlot(PlayerSlot slot) {
    int cameraId = dComIfGp_getPlayerCameraID(static_cast<int>(normalizeSlot(slot)));
    return cameraId >= 0 ? cameraId : 0;
}

view_port_class* viewportForSlot(PlayerSlot slot) {
    dDlst_window_c* window = dComIfGp_getWindow(dComIfGp_getCameraWinID(cameraIdForSlot(slot)));
    return window != NULL ? window->getViewPort() : NULL;
}

void removeViewportProjectionOffset(view_port_class* viewport, Vec* point) {
    if (viewport == NULL || point == NULL) {
        return;
    }

    if (viewport->x_orig != 0.0f) {
        point->x -= (0.5f * ((2.0f * viewport->x_orig) + viewport->width)) - (int)(FB_WIDTH / 2);
    }
    if (viewport->y_orig != 0.0f) {
        point->y -= (0.5f * ((2.0f * viewport->y_orig) + viewport->height)) - (int)(FB_HEIGHT / 2);
    }
}

}  // namespace

PlayerSlot currentSlot() {
    return s_presentationDepth > 0 ? s_presentationStack[s_presentationDepth - 1] : s_singularSlot;
}

int currentPad() {
    return getPadForSlot(currentSlot());
}

void pushPresentationSlot(PlayerSlot slot) {
    if (s_presentationDepth < kPlayerSlotCount) {
        s_presentationStack[s_presentationDepth++] = normalizeSlot(slot);
    }
}

void popPresentationSlot() {
    if (s_presentationDepth > 0) {
        s_presentationDepth--;
    }
}

void retainSingularSlot(PlayerSlot slot) {
    s_singularSlot = normalizeSlot(slot);
}

void clearSingularSlot() {
    s_singularSlot = PlayerSlot::Primary;
}

PlayerSlot singularSlot() {
    return s_singularSlot;
}

bool beginViewport(PlayerSlot slot, ViewportState* state) {
    if (state == NULL) {
        return false;
    }

    view_port_class* viewport = viewportForSlot(slot);
    if (viewport == NULL) {
        return false;
    }

    GXGetViewportv(state->viewport);
    GXGetScissor(&state->scissor[0], &state->scissor[1], &state->scissor[2], &state->scissor[3]);
    GXSetViewport(viewport->x_orig, viewport->y_orig, viewport->width, viewport->height,
                  viewport->near_z, viewport->far_z);
    GXSetScissor(viewport->x_orig, viewport->y_orig, viewport->width, viewport->height);
    return true;
}

void endViewport(const ViewportState& state) {
    GXSetViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3],
                  state.viewport[4], state.viewport[5]);
    GXSetScissor(state.scissor[0], state.scissor[1], state.scissor[2], state.scissor[3]);
}

bool setViewportGraph(PlayerSlot slot, J2DOrthoGraph* graph) {
    if (graph == NULL) {
        return false;
    }

    view_port_class* viewport = viewportForSlot(slot);
    if (viewport == NULL) {
        return false;
    }

    graph->place(viewport->x_orig, viewport->y_orig, viewport->width, viewport->height);
    graph->setOrtho(mDoGph_gInf_c::getMinXF(), mDoGph_gInf_c::getMinYF(),
                    mDoGph_gInf_c::getWidthF(), mDoGph_gInf_c::getHeightF(), 100000.0f,
                    -100000.0f);
    graph->setPort();
    return true;
}

bool projectWorldPointLocal(PlayerSlot slot, const cXyz& point, Vec* out) {
    if (out == NULL) {
        return false;
    }

    int cameraId = cameraIdForSlot(slot);
    camera_process_class* camera = dComIfGp_getCamera(cameraId);
    view_port_class* viewport = viewportForSlot(slot);
    if (camera == NULL || viewport == NULL) {
        return false;
    }

    view_class* oldView = dComIfGd_getView();
    view_port_class* oldViewport = dComIfGd_getViewport();
    dComIfGd_setView(&camera->view);
    dComIfGd_setViewport(viewport);
    cXyz mutablePoint = point;
    mDoLib_project(&mutablePoint, out);
    removeViewportProjectionOffset(viewport, out);
    dComIfGd_setView(oldView);
    dComIfGd_setViewport(oldViewport);
    return true;
}

}  // namespace dusk::coop::ui_owner
