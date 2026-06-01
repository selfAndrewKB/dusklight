#ifndef D_METER_D_METER_HAKUSHA_H
#define D_METER_D_METER_HAKUSHA_H

#include "d/d_meter2.h"

#if TARGET_PC
#include "dusk/coop/player_slots.h"
#endif

class J2DGrafContext;

class dMeterHakusha_c : public dMeterSub_c {
public:
    struct hakusha_data {
        f32 pos_x;
        f32 pos_y;
        u8 flags;
    };

    dMeterHakusha_c(void*);
    void alphaAnimeHakusha(u32);
    void updateHakusha();
    void setAlphaHakushaAnimeMin();
    void setAlphaHakushaAnimeMax();
    void setAlphaButtonAnimeMin();
    void setAlphaButtonAnimeMax();
    int getHakushaNum();

#if TARGET_PC
    // Co-op: each replayed spur presenter retains the native pane-alpha lifecycle for its rider.
    struct coop_alpha_state {
        f32 rate;
        s16 timer;
    };

    struct coop_hakusha_state {
        hakusha_data data[12];
        f32 animFrame[12];
        s16 num;
        u8 status[12];
        coop_alpha_state alpha[3];
    };
#endif

    virtual void draw();
    virtual ~dMeterHakusha_c();
    virtual int _create();
    virtual int _execute(u32);
    virtual int _delete();

private:
    void drawHakushaState(J2DGrafContext*, hakusha_data*, f32*, u8*);
    void updateHakushaState(hakusha_data*, f32*, s16*, u8*, s16);
#if TARGET_PC
    void alphaAnimeHakushaState(u32, u8);
    void captureAlphaState(coop_alpha_state*);
    void applyAlphaState(const coop_alpha_state*);
#endif

public:
    /* 0x004 */ J2DScreen* field_0x004;
    /* 0x008 */ J2DScreen* mpHakushaScreen;
    /* 0x00C */ J2DScreen* mpButtonScreen;
    /* 0x010 */ CPaneMgr* mpHakushaParent;
    /* 0x014 */ CPaneMgr* mpHakushaPos[6];
    /* 0x02C */ CPaneMgr* mpHakushaOn;
    /* 0x030 */ CPaneMgr* mpHakushaOff;
    /* 0x034 */ CPaneMgr* mpButtonA;
    /* 0x038 */ hakusha_data mHakushaData[12];
    /* 0x0C8 */ f32 mHakushaAnimFrame[12];
    /* 0x0F8 */ f32 mButtonAPosX;
    /* 0x0FC */ f32 mButtonAPosY;
    /* 0x100 */ f32 field_0x100;
    /* 0x104 */ f32 field_0x104;
    /* 0x108 */ s16 mHakushaNum;
    /* 0x10A */ u8 mHakushaStatus[12];
#if TARGET_PC
    // Co-op: one native presenter keeps independent animation state for each additional HUD slot.
    coop_hakusha_state mCoopHakushaState[dusk::coop::kPlayerSlotCount - 1];
#endif
};

#endif /* D_METER_D_METER_HAKUSHA_H */
