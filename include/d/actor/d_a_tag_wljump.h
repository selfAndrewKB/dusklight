#ifndef D_A_TAG_WLJUMP_H
#define D_A_TAG_WLJUMP_H

#include "d/d_msg_flow.h"
#include "d/d_com_inf_game.h"

#if TARGET_PC
#include "dusk/coop/player_slots.h"
#endif

struct dPath;
class daAlink_c;

class daTagWljump_c : public fopAc_ac_c {
public:
    int create();
    ~daTagWljump_c();
    int execute();
    int draw();

    const cXyz* getLockPos() const {
        if (field_0x568 < 0) {
            return NULL;
        } else {
            return &eyePos;
        }
    }

    f32 getLandArea() const { return mLandArea; }
    void onNextCheckFlg() { mNextCheckFlg = true; }
    s16 getNotSlideFlg() const { return shape_angle.z; }

#if TARGET_PC
    const cXyz* getLockPos(const daAlink_c* player) const;
    f32 getLandArea(const daAlink_c* player) const;
    void onNextCheckFlg(const daAlink_c* player);
    s16 getNotSlideFlg(const daAlink_c* player) const;
    u32 getAttentionFlags(const daAlink_c* player) const;
    const cXyz& getAttentionPosition(const daAlink_c* player) const;
    bool requiresTutorialMessage();
    bool beginCoopTraversal(daAlink_c* player);
    void releaseCoopApproach(daAlink_c* player);
    void updateCoopPlayerState(daAlink_c* player);
    u8 getCoopApproachPhase(const daAlink_c* player) const;
    bool isCoopTraversalReady(const daAlink_c* player) const;
#endif

    /* 0x568 */ s8 field_0x568;
    /* 0x569 */ s8 field_0x569;
    /* 0x56A */ u8 field_0x56a;
    /* 0x56B */ u8 mNextCheckFlg;
    /* 0x56C */ u8 field_0x56c;
    /* 0x56D */ u8 field_0x56d;
    /* 0x56E */ u8 field_0x56e;
    /* 0x56F */ u8 field_0x56f;
    /* 0x570 */ s8 field_0x570;
    /* 0x571 */ u8 field_0x571;
    /* 0x572 */ u8 field_0x572;
    /* 0x573 */ u8 field_0x573;
    /* 0x574 */ s16 field_0x574;
    /* 0x574 */ u16 field_0x576;
    /* 0x578 */ dMsgFlow_c mMsgFlow;
    /* 0x5C4 */ dPath* field_0x5c4;
    /* 0x5C8 */ f32 mLandArea;

#if TARGET_PC
    // Co-op: each Link retains Midna's native approach until its own jump proc takes custody.
    enum class CoopApproachPhase : u8 {
        Idle,
        Traveling,
        Stationed,
    };

    struct CoopTraversalState {
        const daAlink_c* owner = NULL;
        fpc_ProcID ownerId = fpcM_ERROR_PROCESS_ID_e;
        s8 lockPoint = -1;
        s8 talkPoint = -1;
        u8 currentPoint = 0;
        bool nextCheck = false;
        bool ready = false;
        CoopApproachPhase approachPhase = CoopApproachPhase::Idle;
        s16 noPosFrames = 0;
        cXyz lockPos;
        cXyz attentionPos;
        f32 landArea = 0.0f;
        s16 notSlide = 0;
        u32 attentionFlags = 0;
    };

    CoopTraversalState mCoopTraversal[dusk::coop::kPlayerSlotCount];
#endif
};

#endif /* D_A_TAG_WLJUMP_H */
