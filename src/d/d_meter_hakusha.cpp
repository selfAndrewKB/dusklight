/**
 * d_meter_hakusha.cpp
 * UI Epona Dash Spurs
 */

#include "d/dolzel.h" // IWYU pragma: keep

#include "d/d_meter_hakusha.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "d/d_pane_class.h"
#include <cstring>

#if TARGET_PC
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "dusk/coop/event_presentation.h"
#include "dusk/coop/hud_owner.h"
#include "dusk/coop/ui_owner.h"
#endif

dMeterHakusha_c::dMeterHakusha_c(void* i_screen) {
    field_0x004 = (J2DScreen*)i_screen;
    _create();
}

dMeterHakusha_c::~dMeterHakusha_c() {
    _delete();
}

int dMeterHakusha_c::_create() {
    static u64 haku_tag[] = {
        MULTI_CHAR('haku_n00'), MULTI_CHAR('haku_n01'), MULTI_CHAR('haku_n02'), MULTI_CHAR('haku_n03'), MULTI_CHAR('haku_n04'), MULTI_CHAR('haku_n05'),
    };

    for (int i = 0; i < 6; i++) {
        mpHakushaPos[i] = JKR_NEW CPaneMgr(field_0x004, haku_tag[i], 0, NULL);
        JUT_ASSERT(0, mpHakushaPos[i] != NULL);
    }

    mpHakushaParent = JKR_NEW CPaneMgr(field_0x004, MULTI_CHAR('hakunall'), 0, NULL);
    JUT_ASSERT(0, mpHakushaParent != NULL);

    mpHakushaScreen = JKR_NEW J2DScreen();
    JUT_ASSERT(0, mpHakushaScreen != NULL);

    bool fg = mpHakushaScreen->setPriority("zelda_game_image_hakusha_parts.blo", 0x20000,
                                           dComIfGp_getMain2DArchive());
    JUT_ASSERT(0, fg != false);
    dPaneClass_showNullPane(mpHakushaScreen);

    mpHakushaOn = JKR_NEW CPaneMgr(mpHakushaScreen, MULTI_CHAR('haku_n'), 2, NULL);
    JUT_ASSERT(0, mpHakushaOn != NULL);

    mpHakushaOff = JKR_NEW CPaneMgr(mpHakushaScreen, MULTI_CHAR('haku_b_n'), 2, NULL);
    JUT_ASSERT(0, mpHakushaOff != NULL);

    mpHakushaOn->setAlphaRate(0.0f);
    mpHakushaOff->setAlphaRate(0.0f);
    mpHakushaOn->hide();
    mpHakushaOff->hide();

    for (int i = 0; i < 12; i++) {
        mHakushaData[i].pos_x = 0.0f;
        mHakushaData[i].pos_y = 0.0f;
        mHakushaData[i].flags = 0;
        mHakushaAnimFrame[i] = 0.0f;
        mHakushaStatus[i] = 0;
    }

    mHakushaNum = dMeter2Info_getHorseLifeCount();
#if TARGET_PC
    for (int slot = 1; slot < dusk::coop::kPlayerSlotCount; slot++) {
        coop_hakusha_state& state = mCoopHakushaState[slot - 1];
        std::memset(&state, 0, sizeof(state));
        dusk::coop::hud_owner::pushSlot(static_cast<dusk::coop::PlayerSlot>(slot));
        state.num = dusk::coop::hud_owner::horseLifeCount();
        dusk::coop::hud_owner::popSlot();
    }
#endif

    mpButtonScreen = JKR_NEW J2DScreen();
    JUT_ASSERT(0, mpButtonScreen != NULL);

    fg = mpButtonScreen->setPriority("zelda_game_image_hakusha_a_btn.blo", 0x20000,
                                     dComIfGp_getMain2DArchive());
    JUT_ASSERT(0, fg != false);
    dPaneClass_showNullPane(mpButtonScreen);

    mpButtonA = JKR_NEW CPaneMgr(mpButtonScreen, MULTI_CHAR('abtn_n'), 2, NULL);
    JUT_ASSERT(0, mpButtonA != NULL);
    mpButtonA->show();
    mpButtonA->setAlphaRate(0.0f);

    mpButtonScreen->search(MULTI_CHAR('info_n'))->translate(0.0f, 0.0f);
    field_0x100 = g_drawHIO.mButtonAHorsePosX;
    field_0x104 = g_drawHIO.mButtonAHorsePosY;

    if (strcmp(dComIfGp_getStartStageName(), "F_SP108") == 0) {
        mpHakushaParent->paneTrans(g_drawHIO.mSpurBarPosX + 28.4f, g_drawHIO.mSpurBarPosY);
    } else {
        mpHakushaParent->paneTrans(g_drawHIO.mSpurBarPosX, g_drawHIO.mSpurBarPosY);
    }
    mpHakushaParent->scale(g_drawHIO.mSpurBarScale, g_drawHIO.mSpurBarScale);

    mpButtonA->paneTrans(g_drawHIO.mButtonAHorsePosX, g_drawHIO.mButtonAHorsePosY);
    mpButtonA->scale(g_drawHIO.mButtonAHorseScale, g_drawHIO.mButtonAHorseScale);

    mpHakushaOn->scale(g_drawHIO.mSpurIconScale, g_drawHIO.mSpurIconScale);
    mpHakushaOff->scale(g_drawHIO.mUsedSpurIconScale, g_drawHIO.mUsedSpurIconScale);

    mButtonAPosX = 0.0f;
    mButtonAPosY = 0.0f;
    return cPhs_COMPLEATE_e;
}

int dMeterHakusha_c::_execute(u32 i_flags) {
    updateHakusha();
#if TARGET_PC
    alphaAnimeHakusha(i_flags);
    coop_alpha_state primaryAlpha[3];
    captureAlphaState(primaryAlpha);

    for (int slot = 1; slot < dusk::coop::kPlayerSlotCount; slot++) {
        coop_hakusha_state& state = mCoopHakushaState[slot - 1];
        applyAlphaState(state.alpha);
        dusk::coop::hud_owner::pushSlot(static_cast<dusk::coop::PlayerSlot>(slot));
        updateHakushaState(state.data, state.animFrame, &state.num, state.status,
                           dusk::coop::hud_owner::horseLifeCount());
        // Co-op: advance this rider's native spur visibility using their slot-local prompt state.
        alphaAnimeHakushaState(
            i_flags,
            dusk::coop::hud_owner::buttonStatus(
                dusk::coop::player_button_status::ButtonStatusKind::Do));
        dusk::coop::hud_owner::popSlot();
        captureAlphaState(state.alpha);
    }

    applyAlphaState(primaryAlpha);
#else
    alphaAnimeHakusha(i_flags);
#endif
    return 1;
}

void dMeterHakusha_c::draw() {
    J2DGrafContext* graf_ctx = dComIfGp_getCurrentGrafPort();
    graf_ctx->setup2D();

#if TARGET_PC
    if (!dusk::coop::event_presentation::shouldPresentSplitViewports()) {
        drawHakushaState(graf_ctx, mHakushaData, mHakushaAnimFrame, mHakushaStatus);
        return;
    }

    // Co-op: split-screen replay draws only the riders represented by each viewport.
    dusk::coop::hud_owner::pushSlot(dusk::coop::PlayerSlot::Primary);
    if (dusk::coop::hud_owner::isHorseMeterVisible()) {
        drawHakushaState(graf_ctx, mHakushaData, mHakushaAnimFrame, mHakushaStatus);
    }
    dusk::coop::hud_owner::popSlot();

    for (int slot = 1; slot < dusk::coop::kPlayerSlotCount; slot++) {
        // Co-op: do not let an unallocated extra viewport fall back to P1's camera during replay.
        if (slot >= dComIfGp_getWindowNum()) {
            continue;
        }

        const dusk::coop::PlayerSlot playerSlot = static_cast<dusk::coop::PlayerSlot>(slot);
        dusk::coop::hud_owner::pushSlot(playerSlot);
        if (dusk::coop::hud_owner::isHorseMeterVisible()) {
            dusk::coop::ui_owner::ViewportState viewportState;
            J2DOrthoGraph graph;
            if (dusk::coop::ui_owner::beginViewport(playerSlot, &viewportState)) {
                if (dusk::coop::ui_owner::setViewportGraph(playerSlot, &graph)) {
                    // Co-op: replay the same native spur presenter with this viewport's horse state.
                    coop_hakusha_state& state = mCoopHakushaState[slot - 1];
                    coop_alpha_state primaryAlpha[3];
                    captureAlphaState(primaryAlpha);
                    applyAlphaState(state.alpha);
                    drawHakushaState(&graph, state.data, state.animFrame, state.status);
                    applyAlphaState(primaryAlpha);
                }
                // Co-op: setPort restores the ortho graph but also resets GX viewport state.
                // Reapply the saved P1 viewport afterward so later HUD packets stay in P1's window.
                graf_ctx->setPort();
                dusk::coop::ui_owner::endViewport(viewportState);
            }
        }
        dusk::coop::hud_owner::popSlot();
    }
#else
    drawHakushaState(graf_ctx, mHakushaData, mHakushaAnimFrame, mHakushaStatus);
#endif
}

void dMeterHakusha_c::drawHakushaState(J2DGrafContext* graf_ctx, hakusha_data* data,
                                       f32* animFrame, u8* status) {
    mpButtonA->translate(mButtonAPosX, mButtonAPosY);
    mpButtonScreen->draw(0.0f, 0.0f, graf_ctx);

    for (int i = 0; i < getHakushaNum(); i++) {
        if (data[i].flags & 1) {
            mpHakushaOn->show();
        } else {
            mpHakushaOn->hide();
        }

        if (data[i].flags & 2) {
            mpHakushaOff->show();
        } else {
            mpHakushaOff->hide();
        }

        mpHakushaOn->translate(data[i].pos_x, data[i].pos_y);
        mpHakushaOff->translate(data[i].pos_x, data[i].pos_y);
        mpHakushaScreen->draw(0.0f, 0.0f, graf_ctx);

        if (data[i].flags != 0 && animFrame[i] != 0.0f) {
            Vec center = mpHakushaOn->getGlobalVtxCenter(false, 0);

            if (status[i] == 0) {
                dMeter2Info_getMeterClass()->getMeterDrawPtr()->drawPikariHakusha(
                    center.x, center.y, animFrame[i], g_drawHIO.mSpurIconPikariScale,
                    g_drawHIO.mSpurIconPikariFrontOuter, g_drawHIO.mSpurIconPikariFrontInner,
                    g_drawHIO.mSpurIconPikariBackOuter, g_drawHIO.mSpurIconPikariBackInner);
            } else {
                dMeter2Info_getMeterClass()->getMeterDrawPtr()->drawPikariHakusha(
                    center.x, center.y, animFrame[i],
                    g_drawHIO.mSpurIconRevivePikariScale,
                    g_drawHIO.mSpurIconRevivePikariFrontOuter,
                    g_drawHIO.mSpurIconRevivePikariFrontInner,
                    g_drawHIO.mSpurIconRevivePikariBackOuter,
                    g_drawHIO.mSpurIconRevivePikariBackInner);
            }
        }
    }
}

int dMeterHakusha_c::_delete() {
    for (int i = 0; i < 6; i++) {
        JKR_DELETE(mpHakushaPos[i]);
        mpHakushaPos[i] = NULL;
    }

    mpHakushaParent->paneTrans(0.0f, 0.0f);
    mpHakushaParent->scale(1.0f, 1.0f);
    JKR_DELETE(mpHakushaParent);
    mpHakushaParent = NULL;

    JKR_DELETE(mpHakushaScreen);
    mpHakushaScreen = NULL;

    JKR_DELETE(mpHakushaOn);
    mpHakushaOn = NULL;

    JKR_DELETE(mpHakushaOff);
    mpHakushaOff = NULL;

    JKR_DELETE(mpButtonScreen);
    mpButtonScreen = NULL;

    JKR_DELETE(mpButtonA);
    mpButtonA = NULL;
    return 1;
}

void dMeterHakusha_c::alphaAnimeHakusha(u32 i_flags) {
#if TARGET_PC
    alphaAnimeHakushaState(i_flags, dComIfGp_getDoStatus());
}

void dMeterHakusha_c::alphaAnimeHakushaState(u32 i_flags, u8 doStatus) {
#endif
    if ((i_flags & 0x4000) || (i_flags & 0x40) || (i_flags & 0x100000) || (i_flags & 0x1000) ||
        (i_flags & 8) || (i_flags & 0x10) || (i_flags & 0x20) || (i_flags & 0x04000000) ||
        (i_flags & 0x08000000) || (i_flags & 0x01000000) || !(i_flags & 0x02000000) ||
        (strcmp(dComIfGp_getStartStageName(), "F_SP00") == 0 &&
         dComIfG_play_c::getLayerNo(0) == 4) ||
#if TARGET_PC
        (doStatus != 9 && doStatus != 0))
#else
        (dComIfGp_getDoStatus() != 9 && dComIfGp_getDoStatus() != 0))
#endif
    {
        setAlphaHakushaAnimeMin();
        setAlphaButtonAnimeMin();
        return;
    }

    setAlphaHakushaAnimeMax();
#if TARGET_PC
    if (doStatus == 9) {
#else
    if (dComIfGp_getDoStatus() == 9) {
#endif
        setAlphaButtonAnimeMax();
    } else {
        setAlphaButtonAnimeMin();
    }
}

#if TARGET_PC
void dMeterHakusha_c::captureAlphaState(coop_alpha_state* state) {
    CPaneMgr* panes[] = {mpHakushaOn, mpHakushaOff, mpButtonA};
    for (int i = 0; i < 3; i++) {
        state[i].rate = panes[i]->getAlphaRate();
        state[i].timer = panes[i]->getAlphaTimer();
    }
}

void dMeterHakusha_c::applyAlphaState(const coop_alpha_state* state) {
    CPaneMgr* panes[] = {mpHakushaOn, mpHakushaOff, mpButtonA};
    for (int i = 0; i < 3; i++) {
        panes[i]->setAlphaRate(state[i].rate);
        panes[i]->alphaAnimeStart(state[i].timer);
    }
}
#endif

void dMeterHakusha_c::updateHakusha() {
    updateHakushaState(mHakushaData, mHakushaAnimFrame, &mHakushaNum, mHakushaStatus,
                       dMeter2Info_getHorseLifeCount());
}

void dMeterHakusha_c::updateHakushaState(hakusha_data* data, f32* animFrame, s16* hakushaNum,
                                         u8* status, s16 horseLifeCount) {
    Vec sp2C = mpHakushaPos[0]->getGlobalVtxCenter(false, 0);
    Vec sp20 = mpHakushaPos[5]->getGlobalVtxCenter(false, 0);

    f32 abtn_x_offset = sp2C.x;
    f32 abtn_y_offset = sp2C.y;

    f32 temp_f28 = (sp20.x - sp2C.x) / (f32)getHakushaNum();

    if (*hakushaNum != horseLifeCount) {
        if (*hakushaNum > horseLifeCount) {
            animFrame[horseLifeCount] =
                18.0f - g_drawHIO.mSpurIconPikariAnimSpeed;
            status[horseLifeCount] = 0;
        } else if (*hakushaNum < horseLifeCount) {
            for (int i = *hakushaNum; i < horseLifeCount; i++) {
                animFrame[i] = 18.0f - g_drawHIO.mSpurIconRevivePikariAnimSpeed;
                status[i] = 1;
            }
        }

        *hakushaNum = horseLifeCount;
    }

    for (int i = 0; i < getHakushaNum(); i++) {
        if (animFrame[i] > 0.0f) {
            if (status[i] == 0) {
                animFrame[i] += g_drawHIO.mSpurIconPikariAnimSpeed;
            } else {
                animFrame[i] += g_drawHIO.mSpurIconRevivePikariAnimSpeed;
            }

            if (animFrame[i] > 28.0f) {
                animFrame[i] = 0.0f;
            }
        }

        data[i].pos_x = abtn_x_offset;
        data[i].pos_y = abtn_y_offset;

        if (mpHakushaOn->getAlpha() == 0) {
            data[i].flags &= ~0x1;
        } else if (i < horseLifeCount ||
                   (animFrame[i] != 0.0f && animFrame[i] <= 20.0f && status[i] == 0) ||
                   (animFrame[i] != 0.0f && animFrame[i] > 20.0f && status[i] == 1))
        {
            data[i].flags |= 0x1;
        } else {
            data[i].flags &= ~0x1;
        }

        if (mpHakushaOff->getAlpha() == 0) {
            data[i].flags &= ~0x2;
        } else if (i < horseLifeCount ||
                   (animFrame[i] != 0.0f && animFrame[i] <= 20.0f && status[i] == 0) ||
                   (animFrame[i] != 0.0f && animFrame[i] > 20.0f && status[i] == 1))
        {
            data[i].flags &= ~0x2;
        } else {
            data[i].flags |= 0x2;
        }

        abtn_x_offset += temp_f28;
    }

    mButtonAPosX = abtn_x_offset + field_0x100;
    mButtonAPosY = abtn_y_offset + field_0x104;

    if (g_drawHIO.mSpurDebug) {
        field_0x100 = g_drawHIO.mButtonAHorsePosX;
        field_0x104 = g_drawHIO.mButtonAHorsePosY;

        if (strcmp(dComIfGp_getStartStageName(), "F_SP108") == 0) {
            mpHakushaParent->paneTrans(g_drawHIO.mSpurBarPosX + 28.4f, g_drawHIO.mSpurBarPosY);
        } else {
            mpHakushaParent->paneTrans(g_drawHIO.mSpurBarPosX, g_drawHIO.mSpurBarPosY);
        }
        mpHakushaParent->scale(g_drawHIO.mSpurBarScale, g_drawHIO.mSpurBarScale);

        mpButtonA->paneTrans(g_drawHIO.mButtonAHorsePosX, g_drawHIO.mButtonAHorsePosY);
        mpButtonA->scale(g_drawHIO.mButtonAHorseScale, g_drawHIO.mButtonAHorseScale);

        mpHakushaOn->scale(g_drawHIO.mSpurIconScale, g_drawHIO.mSpurIconScale);
        mpHakushaOff->scale(g_drawHIO.mUsedSpurIconScale, g_drawHIO.mUsedSpurIconScale);
    }
}

void dMeterHakusha_c::setAlphaHakushaAnimeMin() {
    if (mpHakushaOn->getAlphaRate() != 0.0f) {
        mpHakushaOn->setAlphaRate(g_drawHIO.mSpurIconAlpha);
        dMeter2Info_getMeterClass()->getMeterDrawPtr()->setAlphaAnimeMin(mpHakushaOn, 5);
    }

    if (mpHakushaOff->getAlphaRate() != 0.0f) {
        mpHakushaOff->setAlphaRate(g_drawHIO.mUsedSpurIconAlpha);
        dMeter2Info_getMeterClass()->getMeterDrawPtr()->setAlphaAnimeMin(mpHakushaOff, 5);
    }
}

void dMeterHakusha_c::setAlphaHakushaAnimeMax() {
    if (mpHakushaOn->getAlphaRate() != g_drawHIO.mSpurIconAlpha) {
        mpHakushaOn->setAlphaRate(g_drawHIO.mSpurIconAlpha);
        dMeter2Info_getMeterClass()->getMeterDrawPtr()->setAlphaAnimeMax(mpHakushaOn, 5);
    }

    if (mpHakushaOff->getAlphaRate() != g_drawHIO.mUsedSpurIconAlpha) {
        mpHakushaOff->setAlphaRate(g_drawHIO.mUsedSpurIconAlpha);
        dMeter2Info_getMeterClass()->getMeterDrawPtr()->setAlphaAnimeMax(mpHakushaOff, 5);
    }
}

void dMeterHakusha_c::setAlphaButtonAnimeMin() {
    if (mpButtonA->getAlphaRate() != 0.0f) {
        mpButtonA->setAlphaRate(1.0f);
        dMeter2Info_getMeterClass()->getMeterDrawPtr()->setAlphaAnimeMin(mpButtonA, 5);
    }
}

void dMeterHakusha_c::setAlphaButtonAnimeMax() {
    if (mpButtonA->getAlphaRate() != 1.0f) {
        mpButtonA->setAlphaRate(1.0f);
        dMeter2Info_getMeterClass()->getMeterDrawPtr()->setAlphaAnimeMax(mpButtonA, 5);
    }
}

int dMeterHakusha_c::getHakushaNum() {
    int hakusha_num = g_drawHIO.mMaxSpurAmount;
    if (hakusha_num > 12) {
        hakusha_num = 12;
    }

    return hakusha_num;
}
