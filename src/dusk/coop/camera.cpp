#include "dusk/coop/camera.h"

#include "SSystem/SComponent/c_malloc.h"
#include "d/d_com_inf_game.h"
#include "d/d_drawlist.h"
#include "dusk/coop/player_slots.h"
#include "dusk/logging.h"
#include "f_op/f_op_camera_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_graphic.h"

#include <cstdint>

namespace dusk::coop::camera {
namespace {

aurora::Module CoopCameraLog("dusk::coop.camera");

struct CameraSaveInfo {
    cXyz pos;
    cXyz target;
    f32 fovy = 0.0f;
    s16 bank = 0;
};

struct PlayerInfo {
    fopAc_ac_c* player = nullptr;
    s8 cameraId = kSecondaryCameraId;
};

struct SidecarState {
    bool enabled = false;
    bool cameraRequested = false;
    SplitScreenLayout layout = SplitScreenLayout::Vertical;
    dDlst_window_c window;
    camera_class* camera = nullptr;
    s8 winId = kSecondaryWindowId;
    s8 player1Id = kSecondaryPlayerId;
    s8 player2Id = -1;
    u32 attentionStatus = 0;
    f32 zoomScale = 0.0f;
    f32 zoomFocus = 0.0f;
    char* paramFileName = nullptr;
    CameraSaveInfo saveInfo;
    PlayerInfo playerInfo;
};

SidecarState s_state;

bool isSplitIndex(int idx) {
    return idx == kSecondaryCameraId;
}

bool isCameraReady() {
    if (s_state.camera == nullptr) {
        return false;
    }

    camera_process_class* camera = static_cast<camera_process_class*>(s_state.camera);
    return camera->mCamera.field_0xb0c != 0;
}

void setWindowRaw(dDlst_window_c* window, f32 x, f32 y, f32 width, f32 height, f32 nearZ,
                  f32 farZ, int cameraId, int mode) {
    window->setViewPort(x, y, width, height, nearZ, farZ);
    window->setScissor(x, y, width, height);
    window->setCameraID(cameraId);
    window->setMode(mode);
}

void applyWindowLayoutForCamera(int cameraId);

void applyWindowLayout() {
    applyWindowLayoutForCamera(kPrimaryCameraId);
    if (s_state.enabled) {
        applyWindowLayoutForCamera(kSecondaryCameraId);
    }
}

void applyWindowLayoutForCamera(int cameraId) {
    if (!s_state.enabled || cameraId == kPrimaryCameraId) {
        if (!s_state.enabled) {
            dComIfGp_setWindow(0, 0.0f, 0.0f, FB_WIDTH, FB_HEIGHT, 0.0f, 1.0f,
                               kPrimaryCameraId, 2);
        } else if (s_state.layout == SplitScreenLayout::Horizontal) {
            const f32 halfHeight = static_cast<f32>(FB_HEIGHT) * 0.5f;
            dComIfGp_setWindow(0, 0.0f, 0.0f, FB_WIDTH, halfHeight, 0.0f, 1.0f,
                               kPrimaryCameraId, 2);
        } else {
            const f32 halfWidth = static_cast<f32>(FB_WIDTH) * 0.5f;
            dComIfGp_setWindow(0, 0.0f, 0.0f, halfWidth, FB_HEIGHT, 0.0f, 1.0f,
                               kPrimaryCameraId, 2);
        }
        return;
    }

    if (cameraId != kSecondaryCameraId) {
        return;
    }

    if (s_state.layout == SplitScreenLayout::Horizontal) {
        const f32 halfHeight = static_cast<f32>(FB_HEIGHT) * 0.5f;
        setWindowRaw(&s_state.window, 0.0f, halfHeight, FB_WIDTH, halfHeight, 0.0f, 1.0f,
                     kSecondaryCameraId, 2);
    } else {
        const f32 halfWidth = static_cast<f32>(FB_WIDTH) * 0.5f;
        setWindowRaw(&s_state.window, halfWidth, 0.0f, halfWidth, FB_HEIGHT, 0.0f, 1.0f,
                     kSecondaryCameraId, 2);
    }
}

}  // namespace

bool isSplitScreenEnabled() {
    return s_state.enabled;
}

void setSplitScreenEnabled(bool enabled) {
    if (s_state.enabled == enabled) {
        return;
    }

    s_state.enabled = enabled;
    applyWindowLayout();
    if (enabled) {
        syncSecondaryPlayerAssignment();
        ensureSecondaryCamera();
    }
}

SplitScreenLayout getSplitScreenLayout() {
    return s_state.layout;
}

void setSplitScreenLayout(SplitScreenLayout layout) {
    if (s_state.layout == layout) {
        return;
    }

    s_state.layout = layout;
    applyWindowLayout();
}

void resetSplitScreenCameraState() {
    s_state.enabled = false;
    s_state.cameraRequested = false;
    s_state.camera = nullptr;
    s_state.attentionStatus = 0;
    s_state.zoomScale = 0.0f;
    s_state.zoomFocus = 0.0f;
    s_state.paramFileName = nullptr;
    s_state.saveInfo = {};
    s_state.playerInfo = {};
    s_state.playerInfo.cameraId = kSecondaryCameraId;
    applyWindowLayout();
}

bool ensureSecondaryCamera() {
    if (!s_state.enabled) {
        return false;
    }

    syncSecondaryPlayerAssignment();
    if (s_state.playerInfo.player == nullptr) {
        return false;
    }

    applyWindowLayout();
    setCameraInfo(kSecondaryCameraId, s_state.camera, kSecondaryWindowId, kSecondaryPlayerId, -1);
    setPlayerInfo(kSecondaryPlayerId, s_state.playerInfo.player, kSecondaryCameraId);

    if (s_state.camera != nullptr || s_state.cameraRequested) {
        return isCameraReady();
    }

    fopCamM_prm_class* params =
        static_cast<fopCamM_prm_class*>(cMl::memalignB(-4, sizeof(fopCamM_prm_class)));
    if (params == nullptr) {
        CoopCameraLog.warn("failed to allocate secondary camera params");
        return false;
    }

    params->base.position.x = 0.0f;
    params->base.position.y = 0.0f;
    params->base.position.z = 0.0f;
    params->base.parameters = kSecondaryCameraId;
    s_state.cameraRequested = fopCamM_Create(kSecondaryCameraId, fpcNm_CAMERA2_e, params) != 0;
    if (!s_state.cameraRequested) {
        CoopCameraLog.warn("failed to request secondary camera");
    }
    return isCameraReady();
}

void syncSecondaryPlayerAssignment() {
    fopAc_ac_c* player = dusk::coop::getPlayer(dusk::coop::PlayerSlot::Secondary);
    if (s_state.playerInfo.player == player) {
        return;
    }

    s_state.playerInfo.player = player;
    s_state.playerInfo.cameraId = kSecondaryCameraId;
    if (player != nullptr) {
        setCameraInfo(kSecondaryCameraId, s_state.camera, kSecondaryWindowId, kSecondaryPlayerId, -1);
    }
}

void refreshWindowLayout() {
    applyWindowLayout();
}

void refreshWindowLayoutForCamera(int cameraId) {
    applyWindowLayoutForCamera(cameraId);
}

bool isExtensionIndex(int idx) {
    return isSplitIndex(idx);
}

int getEffectiveWindowNum(int baseWindowNum) {
    if (baseWindowNum == 0 || !s_state.enabled) {
        return baseWindowNum;
    }

    return isCameraReady() && s_state.playerInfo.player != nullptr ? 2 : baseWindowNum;
}

dDlst_window_c* getWindow(int idx) {
    return isSplitIndex(idx) ? &s_state.window : nullptr;
}

void setWindow(int idx, f32 x, f32 y, f32 width, f32 height, f32 nearZ, f32 farZ, int camId,
               int mode) {
    if (!isSplitIndex(idx)) {
        return;
    }

    setWindowRaw(&s_state.window, x, y, width, height, nearZ, farZ, camId, mode);
}

camera_class* getCamera(int idx) {
    return isSplitIndex(idx) ? s_state.camera : nullptr;
}

void setCamera(int idx, camera_class* camera) {
    if (!isSplitIndex(idx)) {
        return;
    }

    s_state.camera = camera;
    if (camera == nullptr) {
        s_state.cameraRequested = false;
    }
}

int getCameraWinID(int idx) {
    return isSplitIndex(idx) ? s_state.winId : 0;
}

int getCameraPlayer1ID(int idx) {
    return isSplitIndex(idx) ? s_state.player1Id : 0;
}

int getCameraPlayer2ID(int idx) {
    return isSplitIndex(idx) ? s_state.player2Id : -1;
}

u32 getCameraAttentionStatus(int idx) {
    return isSplitIndex(idx) ? s_state.attentionStatus : 0;
}

BOOL checkCameraAttentionStatus(int idx, u32 flag) {
    return isSplitIndex(idx) ? (s_state.attentionStatus & flag) : FALSE;
}

void setCameraAttentionStatus(int idx, u32 flag) {
    if (isSplitIndex(idx)) {
        s_state.attentionStatus = flag;
    }
}

void onCameraAttentionStatus(int idx, u32 flag) {
    if (isSplitIndex(idx)) {
        s_state.attentionStatus |= flag;
    }
}

void offCameraAttentionStatus(int idx, u32 flag) {
    if (isSplitIndex(idx)) {
        s_state.attentionStatus &= ~flag;
    }
}

void setCameraInfo(int idx, camera_class* camera, int winId, int player1Id, int player2Id) {
    if (!isSplitIndex(idx)) {
        return;
    }

    s_state.camera = camera;
    s_state.winId = static_cast<s8>(winId);
    s_state.player1Id = static_cast<s8>(player1Id);
    s_state.player2Id = static_cast<s8>(player2Id);
    s_state.attentionStatus = 0;
}

f32 getCameraZoomScale(int idx) {
    return isSplitIndex(idx) ? s_state.zoomScale : 0.0f;
}

void setCameraZoomScale(int idx, f32 scale) {
    if (isSplitIndex(idx)) {
        s_state.zoomScale = scale;
    }
}

f32 getCameraZoomForcus(int idx) {
    return isSplitIndex(idx) ? s_state.zoomFocus : 0.0f;
}

void setCameraZoomForcus(int idx, f32 focus) {
    if (isSplitIndex(idx)) {
        s_state.zoomFocus = focus;
    }
}

const char* getCameraParamFileName(int idx) {
    return isSplitIndex(idx) ? s_state.paramFileName : nullptr;
}

void setCameraParamFileName(int idx, char* name) {
    if (isSplitIndex(idx)) {
        s_state.paramFileName = name;
    }
}

void saveCameraPosition(int idx, cXyz* pos, cXyz* target, f32 fovy, s16 bank) {
    if (!isSplitIndex(idx)) {
        return;
    }

    s_state.saveInfo.pos = *pos;
    s_state.saveInfo.target = *target;
    s_state.saveInfo.fovy = fovy;
    s_state.saveInfo.bank = bank;
}

void loadCameraPosition(int idx, cXyz* pos, cXyz* target, f32* fovy, s16* bank) {
    if (!isSplitIndex(idx)) {
        return;
    }

    *pos = s_state.saveInfo.pos;
    *target = s_state.saveInfo.target;
    *fovy = s_state.saveInfo.fovy;
    *bank = s_state.saveInfo.bank;
}

fopAc_ac_c* getPlayer(int idx) {
    return isSplitIndex(idx) ? s_state.playerInfo.player : nullptr;
}

void setPlayer(int idx, fopAc_ac_c* player) {
    if (isSplitIndex(idx)) {
        s_state.playerInfo.player = player;
    }
}

int getPlayerCameraID(int idx) {
    return isSplitIndex(idx) ? s_state.playerInfo.cameraId : 0;
}

void setPlayerInfo(int idx, fopAc_ac_c* player, int cameraId) {
    if (!isSplitIndex(idx)) {
        return;
    }

    s_state.playerInfo.player = player;
    s_state.playerInfo.cameraId = static_cast<s8>(cameraId);
}

f32 getWindowAspect(int cameraId) {
    const int windowId = isSplitIndex(cameraId) ? s_state.winId : kPrimaryCameraId;
    dDlst_window_c* window = isSplitIndex(windowId) ? &s_state.window : dComIfGp_getWindow(0);
    if (window == nullptr) {
        return mDoGph_gInf_c::getAspect();
    }

    view_port_class* viewport = window->getViewPort();
    if (viewport == nullptr || viewport->height == 0.0f) {
        return mDoGph_gInf_c::getAspect();
    }

    return viewport->width / viewport->height;
}

bool isSecondaryCameraReady() {
    return isCameraReady();
}

bool isSecondaryCameraRequested() {
    return s_state.cameraRequested;
}

}  // namespace dusk::coop::camera
