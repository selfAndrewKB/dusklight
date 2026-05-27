#include "d/dolzel.h" // IWYU pragma: keep

#include "d/d_scope.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#if TARGET_PC
#include "d/d_drawlist.h"
#include "dusk/coop/camera.h"
#include "dusk/coop/player_camera_status.h"
#endif

typedef void (dScope_c::*initFunc)();
initFunc init_process[] = {
    &dScope_c::open_init,
    &dScope_c::move_init,
    &dScope_c::close_init,
};

typedef void (dScope_c::*moveFunc)();
moveFunc move_process[] = {
    &dScope_c::open_proc,
    &dScope_c::move_proc,
    &dScope_c::close_proc,
};

dScope_c::dScope_c(u8 param_0) : field_0x58(-1), field_0x5c(-1) {
    field_0x8d = param_0;
    ResTIMG* mp_image = (ResTIMG*)dComIfGp_getMain2DArchive()->getResource('TIMG', "wipe_00.bti");

    mHawkEyeScrn = NULL;
    mHawkEyeRootPane = NULL;
    mZoomInOutScrn = NULL;
    mZoomInOutRootPane = NULL;

    for (int i = 0; i < 3; i++) {
        mHawkEyePanes[i] = NULL;
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            mZoomInOutPanes[i][j] = NULL;
        }
    }

    mpWipeTex = JKR_NEW J2DPicture(mp_image);
    mpWipeTex->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
    mWidth = mp_image->width;
    mHeight = mp_image->height;

    mp_image = (ResTIMG*)dComIfGp_getMain2DArchive()->getResource('TIMG', "tt_block8x8.bti");
    mpBlackTex = JKR_NEW J2DPicture(mp_image);
    mpBlackTex->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 255));
    mScale = 3.0f;
    mAlpha = 0.0f;
    mOpenTimer = 0;
    field_0x8a = 0;
    mProcess = PROC_OPEN;
    mIsDead = false;
    (this->*init_process[mProcess])();
}

dScope_c::~dScope_c() {
    if (mHawkEyeScrn != NULL) {
        JKR_DELETE(mHawkEyeScrn);
        mHawkEyeScrn = NULL;
    }

    if (mHawkEyeRootPane != NULL) {
        JKR_DELETE(mHawkEyeRootPane);
        mHawkEyeRootPane = NULL;
    }

    for (int i = 0; i < 3; i++) {
        if (mHawkEyePanes[i] != NULL) {
            JKR_DELETE(mHawkEyePanes[i]);
            mHawkEyePanes[i] = NULL;
        }
    }

    if (mZoomInOutScrn != NULL) {
        JKR_DELETE(mZoomInOutScrn);
        mZoomInOutScrn = NULL;
    }

    if (mZoomInOutRootPane != NULL) {
        JKR_DELETE(mZoomInOutRootPane);
        mZoomInOutRootPane = NULL;
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            if (mZoomInOutPanes[i][j] != NULL) {
                JKR_DELETE(mZoomInOutPanes[i][j]);
                mZoomInOutPanes[i][j] = NULL;
            }
        }
    }

    JKR_DELETE(mpWipeTex);
    mpWipeTex = NULL;

    JKR_DELETE(mpBlackTex);
    mpBlackTex = NULL;

    dMeter2Info_setScopeZoomPointer(0);
}

int dScope_c::_execute(u32) {
    u8 old_proc = mProcess;
    (this->*move_process[mProcess])();

#if TARGET_PC
    // Co-op: Hawkeye lifetime is owned by the player's camera row, not always camera 0.
    int camera_id = dComIfGp_getPlayerCameraID(field_0x8d);
    if (camera_id < 0) {
        camera_id = 0;
    }
#else
    int camera_id = 0;
#endif
    if (!dComIfGp_checkCameraAttentionStatus(camera_id, 8)) {
        mProcess = PROC_CLOSE;
    }

    if (mProcess != old_proc) {
        (this->*init_process[mProcess])();
    }

    if (mProcess != PROC_CLOSE) {
        dComIfGp_setCStickStatusForce(61, 10, 3);

#if TARGET_PC
        if (dusk::coop::player_camera_status::checkStatus0ForPlayerId(field_0x8d, 0x1000)) {
#else
        if (dComIfGp_checkPlayerStatus0(0, 0x1000)) {
#endif
            dComIfGp_setRStatusForce(0x11, 3);
        }
    }

    return 1;
}

void dScope_c::draw() {
    dComIfGp_getCurrentGrafPort()->setup2D();
#if TARGET_PC
    f32 saved_viewport[6];
    u32 saved_scissor[4];
    bool restore_viewport = false;
    if (dusk::coop::camera::isSplitScreenEnabled()) {
        int camera_id = dComIfGp_getPlayerCameraID(field_0x8d);
        if (camera_id < 0) {
            camera_id = 0;
        }

        // Co-op: scope is a view overlay, so draw it into the owner's split viewport
        // instead of the shared meter pass's P1 HUD viewport, then restore HUD state.
        GXGetViewportv(saved_viewport);
        GXGetScissor(&saved_scissor[0], &saved_scissor[1], &saved_scissor[2], &saved_scissor[3]);
        restore_viewport = true;
        view_port_class* view_port = dComIfGp_getWindow(dComIfGp_getCameraWinID(camera_id))->getViewPort();
        GXSetViewport(view_port->x_orig, view_port->y_orig, view_port->width, view_port->height,
                      view_port->near_z, view_port->far_z);
        GXSetScissor(view_port->x_orig, view_port->y_orig, view_port->width, view_port->height);
    }
#endif
    f32 temp_f1 = mScale;
    f32 temp_f31 = mWidth * temp_f1;
    f32 temp_f30 = mHeight * temp_f1;
    u8 alpha = mAlpha * 255.0f;

#if TARGET_PC
    // Co-op: the red scope reticle follows the scoped player's aim status.
    if (dusk::coop::player_camera_status::checkStatus0ForPlayerId(field_0x8d, 0x1000)) {
#else
    if (dComIfGp_checkPlayerStatus0(0, 0x1000)) {
#endif
        J2DDrawLine(FB_WIDTH_BASE / 2, mDoGph_gInf_c::getMinYF(), FB_WIDTH_BASE / 2,
                    mDoGph_gInf_c::getMaxYF(), JUtility::TColor(255, 0, 0, alpha), 6);
        J2DDrawLine(mDoGph_gInf_c::getMinXF(), FB_HEIGHT_BASE / 2, mDoGph_gInf_c::getMaxXF(),
                    FB_HEIGHT_BASE / 2, JUtility::TColor(255, 0, 0, alpha), 6);
    }

    mpWipeTex->setAlpha(alpha);
    mpBlackTex->setAlpha(alpha);

    f32 temp_f29 = FB_WIDTH_BASE / 2 - temp_f31;
    f32 temp_f28 = FB_WIDTH_BASE / 2 + temp_f31;
    f32 temp_f27 = FB_HEIGHT_BASE / 2 - temp_f30;
    f32 temp_f26 = FB_HEIGHT_BASE / 2 + temp_f30;

    mpWipeTex->draw(temp_f29, temp_f27, temp_f31, temp_f30, false, false, false);
    mpWipeTex->draw(FB_WIDTH_BASE / 2, temp_f27, temp_f31, temp_f30, true, false, false);
    mpWipeTex->draw(temp_f29, FB_HEIGHT_BASE / 2, temp_f31, temp_f30, false, true, false);
    mpWipeTex->draw(FB_WIDTH_BASE / 2, FB_HEIGHT_BASE / 2, temp_f31, temp_f30, true, true, false);

    mpBlackTex->draw(mDoGph_gInf_c::getMinXF(), mDoGph_gInf_c::getMinYF(),
                     mDoGph_gInf_c::getWidthF(), temp_f27 - mDoGph_gInf_c::getMinYF(), false, false,
                     false);
    mpBlackTex->draw(mDoGph_gInf_c::getMinXF(), temp_f26, mDoGph_gInf_c::getWidthF(),
                     mDoGph_gInf_c::getMaxYF() - temp_f26, false, false, false);
    mpBlackTex->draw(mDoGph_gInf_c::getMinXF(), temp_f27, temp_f29 - mDoGph_gInf_c::getMinXF(),
                     temp_f26 - temp_f27, false, false, false);
    mpBlackTex->draw(temp_f28, temp_f27, mDoGph_gInf_c::getMaxXF() - temp_f28, temp_f26 - temp_f27,
                     false, false, false);
#if TARGET_PC
    if (restore_viewport) {
        GXSetViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3],
                      saved_viewport[4], saved_viewport[5]);
        GXSetScissor(saved_scissor[0], saved_scissor[1], saved_scissor[2], saved_scissor[3]);
    }
#endif
}


bool dScope_c::isDead() {
    return mIsDead != false ? 1 : 0;
}

void dScope_c::open_init() {
    mScale = 3.0f;
    mAlpha = 0.0f;
    mOpenTimer = 0;
}

void dScope_c::open_proc() {
    mOpenTimer++;
    mScale = 3.0f - (mOpenTimer / 5.0f) * 1.5f;
    mAlpha = 1.0f;

    if (mOpenTimer >= 5) {
        mScale = 1.5f;
        mAlpha = 1.0f;
        mProcess = PROC_MOVE;
    }
}

void dScope_c::move_init() {}

void dScope_c::move_proc() {}

void dScope_c::close_init() {}

void dScope_c::close_proc() {
    if (mOpenTimer > 0) {
        mOpenTimer--;
        mScale = 3.0f - (mOpenTimer / 5.0f) * 1.5f;
        mAlpha = 1.0f;
    } else {
        mScale = 3.0f;
        mAlpha = 0.0f;
        mIsDead = true;
    }
}
