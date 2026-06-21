/**
 * @file d_a_ykgr.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_ykgr.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "Z2AudioLib/Z2Instances.h"
#include <cstring>

#if TARGET_PC
#include "dusk/coop/player_slots.h"
#include "dusk/coop/render_effects.h"
#endif

struct daYkgr_HIO_c : public mDoHIO_entry_c {
    daYkgr_HIO_c();
    virtual ~daYkgr_HIO_c() {}

    void genMessage(JORMContext*);

    /* 0x04 */ u8 field_0x4;
    /* 0x08 */ s32 field_0x8;
    /* 0x0C */ s32 field_0xc;
    /* 0x10 */ f32 field_0x10;
    /* 0x14 */ f32 field_0x14;
    /* 0x18 */ f32 field_0x18;
    /* 0x1C */ f32 field_0x1c;
    /* 0x20 */ f32 field_0x20;
    /* 0x24 */ f32 field_0x24;
};

#if DEBUG
void daYkgr_HIO_c::genMessage(JORMContext* ctx) {
    ctx->genLabel("竜の山陽炎HIO", 0);
    ctx->genSlider("アルファ速度", &field_0xc, 0, 255);
    ctx->genCheckBox("アルファチェック", &field_0x4, 0x01);
    ctx->genSlider("アルファ", &field_0x8, 0, 255);
    ctx->genSlider("INDMTX PARAM", &field_0x10, -17.0f, 47.0f);
    ctx->genSlider("min_param", &field_0x14, -17.0f, 47.0f);
    ctx->genSlider("max_param", &field_0x18, -17.0f, 47.0f);
    ctx->genSlider("default_rate", &field_0x1c, 0.0f, 1.0f);
    ctx->genSlider("outside_range", &field_0x20, 0.0f, 10000.0f);
    ctx->genSlider("inside_range", &field_0x24, 0.0f, 10000.0f);
}
#endif

inline daYkgr_HIO_c::daYkgr_HIO_c() {
    field_0x4 = 0;
    field_0x8 = 0xff;
    field_0xc = 3;
    field_0x10 = -16.0f;
    field_0x14 = -5.0f;
    field_0x18 = -3.0f;
    field_0x1c = 0.0f;
    field_0x20 = 1500.0f;
    field_0x24 = 500.0f;
}

inline dPa_YkgrPcallBack::dPa_YkgrPcallBack() {
    field_0x4[0][0] = 0.5f;
    field_0x4[0][1] = 0.0f;
    field_0x4[0][2] = 0.0f;
    field_0x4[1][0] = 0.0f;
    field_0x4[1][1] = 0.5f;
    field_0x4[1][2] = 0.0f;
    field_0x1c = 1;
}

void dPa_YkgrPcallBack::draw(JPABaseEmitter* param_0, JPABaseParticle* param_1) {
    GXSetIndTexMtx(GX_ITM_0, field_0x4, field_0x1c);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
}

void dPa_YkgrPcallBack::setParam(f32 param_1) {
    if (param_1 <= -17.0f || param_1 >= 47.0f) return; {
        if (param_1 >= 0.0f) {
            field_0x1c = param_1;
            field_0x4[0][0] = field_0x4[1][1] = (param_1 - field_0x1c) * 0.5f + 0.5f;
        } else {
            param_1 = (param_1 - 1.0f);
            field_0x1c = param_1;
            field_0x4[0][0] = field_0x4[1][1] = (param_1 - field_0x1c) * 0.5f + 1.0f;
        }
        field_0x4[0][1] = 0.0f;
        field_0x4[0][2] = 0.0f;
        field_0x4[1][0] = 0.0f;
        field_0x4[1][2] = 0.0f;
    }
}

static daYkgr_HIO_c l_HIO;

#if TARGET_PC
// Co-op: keep each actor's path-strength smoothing slot-local without changing its layout.
constexpr int kSlotStateCapacity = 8;

struct YkgrSlotState {
    const daYkgr_c* actor = nullptr;
    f32 posRate[dusk::coop::kPlayerSlotCount] = {};
};

static YkgrSlotState s_slotStates[kSlotStateCapacity] = {};

static YkgrSlotState* getSlotState(const daYkgr_c* actor) {
    YkgrSlotState* available = nullptr;
    for (int i = 0; i < kSlotStateCapacity; i++) {
        if (s_slotStates[i].actor == actor) {
            return &s_slotStates[i];
        }
        if (available == nullptr && s_slotStates[i].actor == nullptr) {
            available = &s_slotStates[i];
        }
    }
    if (available != nullptr) {
        *available = {};
        available->actor = actor;
    }
    return available;
}

static void clearSlotState(const daYkgr_c* actor) {
    for (int i = 0; i < kSlotStateCapacity; i++) {
        if (s_slotStates[i].actor == actor) {
            s_slotStates[i] = {};
            return;
        }
    }
}
#endif

static f32 getPosRateForPlayer(const fopAc_ac_c* player) {
    if (daYkgr_c::m_path == NULL || player == NULL) {
        return 0.0f;
    }

    f32 distance = FLT_MAX;
    dPnt* point = daYkgr_c::m_path->m_points;
    for (int i = 0; i < daYkgr_c::m_path->m_num; i++, point++) {
        cXyz pointPos(point->m_position.x, point->m_position.y, point->m_position.z);
        const f32 pointDistance = player->current.pos.absXZ(pointPos);
        if (pointDistance < distance) {
            distance = pointDistance;
        }
    }

    if (distance > l_HIO.field_0x20) {
        distance = l_HIO.field_0x20;
    } else if (distance < l_HIO.field_0x24) {
        distance = l_HIO.field_0x24;
    }
    return 1.0f -
           ((distance - l_HIO.field_0x24) / (l_HIO.field_0x20 - l_HIO.field_0x24));
}

f32 daYkgr_c::getPosRate() {
    return getPosRateForPlayer(dComIfGp_getPlayer(0));
}

static dPa_YkgrPcallBack YkgrCB;

#if TARGET_PC
static f32 getPresentationParam(const daYkgr_c* actor, dusk::coop::PlayerSlot slot) {
    int slotIndex = static_cast<int>(slot);
    if (slotIndex < 0 || slotIndex >= dusk::coop::kPlayerSlotCount) {
        slotIndex = 0;
    }
    YkgrSlotState* state = getSlotState(actor);
    const f32 positionRate = slotIndex == 0 || state == nullptr
                                 ? actor->field_0x5a8
                                 : state->posRate[slotIndex];
    const f32 rate = actor->field_0x5a4 * 0.5f + positionRate * 0.5f;
    return rate * l_HIO.field_0x18 + (1.0f - rate) * l_HIO.field_0x14;
}

static void prepareViewportEmitter(JPABaseEmitter*, dusk::coop::PlayerSlot slot,
                                   void* context) {
    YkgrCB.setParam(getPresentationParam(static_cast<daYkgr_c*>(context), slot));
}

static void restoreViewportEmitter(JPABaseEmitter*, void* context) {
    YkgrCB.setParam(getPresentationParam(static_cast<daYkgr_c*>(context),
                                        dusk::coop::PlayerSlot::Primary));
}
#endif

inline int daYkgr_c::_create() {
    int uVar1 = u8((fopAcM_GetParam(this) & 0x00F00000) >> 0x14);
    fopAcM_ct(this, daYkgr_c);
#if TARGET_PC
    (void)getSlotState(this);
#endif
    u8 uVar4 = (fopAcM_GetParam(this) & 0x0000FF00) >> 8;
    OS_REPORT("pathNo = %d\n", uVar4);
    if (uVar4 != 0xff) {
        m_path = dPath_GetRoomPath(uVar4, fopAcM_GetRoomNo(this));
    } else {
        m_path = NULL;
    }

    if (m_emitter == NULL) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        OS_REPORT("##\n##\n## particle set Ykgr\n##\n##\n");
        this->current.pos = player->current.pos;
        m_emitter = dComIfGp_particle_set(0x80e2, &this->current.pos, NULL, NULL);
        if (m_emitter != NULL) {
            m_emitter->setParticleCallBackPtr(&YkgrCB);
            YkgrCB.setParam(-3.0f);
        } else {
            OS_REPORT("エミッターの生成に失敗しました！！\n");
            return cPhs_ERROR_e;
        }
        fopAcM_setStageLayer(this);
        if (uVar1 == 1) {
            setAlpha(0xff);
            m_emitter->setGlobalAlpha(0xff);
            field_0x5a4 = 0.5f;
            field_0x5a8 = 0.0f;
            start();
        } else {
            setAlpha(0);
            m_emitter->setGlobalAlpha(0);
            field_0x5a4 = 0.5f;
            field_0x5a8 = 0.0f;
            stop();
        }
#if DEBUG
        l_HIO.entryHIO("竜の山陽炎");
#endif
    } else {
        if (uVar1 == 1) {
            start();
        } else {
            stop();
        }
        return cPhs_UNK3_e;
    }
    OS_REPORT(" alpha = %d\n", m_emitter->getGlobalAlpha());
    OS_REPORT(" rate  = %3.3f\n", m_emitter->getRate());
    OS_REPORT("\n\nfopAcM_GetParam = %x\n\n\n", fopAcM_GetParam(this));
    return cPhs_COMPLEATE_e;
}

static int daYkgrCreate(void* i_this) {
    return static_cast<daYkgr_c*>(i_this)->_create();
}

inline int daYkgr_c::_delete() {
#if TARGET_PC
    clearSlotState(this);
#endif
#if DEBUG
    l_HIO.removeHIO();
#endif
    return 1;
}

static int daYkgrDelete(void* i_this) {
    return static_cast<daYkgr_c*>(i_this)->_delete();
}

inline int daYkgr_c::_execute() {
    cLib_addCalc2(&field_0x5a4, m_aim_rate, 0.25f, 0.05f);
    cLib_addCalc2(&m_aim_rate, l_HIO.field_0x1c, 0.25f, 0.05f);
    cLib_addCalc2(&field_0x5a8, getPosRate(), 0.25f, 0.05f);
#if TARGET_PC
    YkgrSlotState* state = getSlotState(this);
    if (state != nullptr) {
        state->posRate[0] = field_0x5a8;
        for (int i = 1; i < dusk::coop::kPlayerSlotCount; i++) {
            fopAc_ac_c* player =
                dusk::coop::getPlayer(static_cast<dusk::coop::PlayerSlot>(i));
            const f32 target = getPosRateForPlayer(player);
            cLib_addCalc2(&state->posRate[i], target, 0.25f, 0.05f);
        }
    }
#endif
    f32 fVar4 = field_0x5a4 * 0.5f + field_0x5a8 * 0.5f;
    f32 tmp = fVar4 * l_HIO.field_0x18 + (1.0f - fVar4) * l_HIO.field_0x14;
    YkgrCB.setParam(tmp);
    if (m_alpha_flag == 0) {
        if (m_alpha > 0) {
            if (m_alpha > l_HIO.field_0xc) {
                // this doesn't feel right but it looks an awful lot
                // like the cast only exists in Shield+ShieldD
#if PLATFORM_SHIELD
                m_alpha -= (u8)l_HIO.field_0xc;
#else
                m_alpha -= l_HIO.field_0xc;
#endif
            } else {
                m_alpha = 0;
            }
        }
    } else {
        if (m_alpha < 0xff) {
            if (m_alpha < 0xff - l_HIO.field_0xc) {
#if PLATFORM_SHIELD
                m_alpha += (u8)l_HIO.field_0xc;
#else
                m_alpha += l_HIO.field_0xc;
#endif
            } else {
                m_alpha = 0xff;
            }
        }
    }
    return 1;
}

static int daYkgrExecute(void* i_this) {
    return static_cast<daYkgr_c*>(i_this)->_execute();
}

void daYkgr_c::set_mtx() {
    camera_process_class* iVar1 = dComIfGp_getCamera(0);
    cXyz local_28;
    cXyz* r29 = fopCamM_GetEye_p(iVar1);
    current.pos = *r29;
    dKyr_get_vectle_calc(&iVar1->view.lookat.eye, &iVar1->view.lookat.center, &local_28);
    current.angle.y = (s16)cM_atan2s(local_28.x, local_28.z);
    current.angle.x = -cM_atan2s(
        local_28.y, JMAFastSqrt((local_28.x * local_28.x + local_28.z * local_28.z)));
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y,
                           current.pos.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mDoMtx_stack_c::XrotM(current.angle.x);
    MTXCopy(mDoMtx_stack_c::get(), field_0x570);
}

// typically we would expect an int return type, but debug seems to indicate this returns a bool
inline bool daYkgr_c::_draw() {
    f32 alpha = 255.0f;
    if (strcmp(dComIfGp_getStartStageName(), "D_MN04A") == 0) {
        int dummy; // force stack pointer into r31 on debug

        alpha = dComIfGs_BossLife_public_Get() / 100.0f;
        m_alpha = alpha * 255.0f;
        if (m_alpha == 0) {
            m_alpha++;
        }
    }
    if (m_alpha == 0) {
        return true;
    } else {
        set_mtx();
        if  (m_emitter != NULL) {
            m_emitter->setGlobalRTMatrix(field_0x570);
            if (DEBUG && l_HIO.field_0x4 != 0) {
                m_emitter->setGlobalAlpha(l_HIO.field_0x8);
                YkgrCB.setParam(l_HIO.field_0x10);
            } else {
                m_emitter->setGlobalAlpha(m_alpha);
            }
#if TARGET_PC
            // Co-op: reinterpret this Camera-0-simulated screen effect for each viewport and
            // apply the presenting player's native path-distance strength before particle draw.
            dusk::coop::render_effects::registerCameraRelativeEmitter(
                m_emitter, dusk::coop::render_effects::cameraIdForSlot(
                               dusk::coop::PlayerSlot::Primary),
                this, prepareViewportEmitter, restoreViewportEmitter);
#endif
        }
        return true;
    }
}

static int daYkgrDraw(void* i_this) {
    return static_cast<daYkgr_c*>(i_this)->_draw();
}

static int daYkgrIsDelete(void* param_0) {
    return 1;
}

static DUSK_CONST actor_method_class daYkgrMethodTable = {
    daYkgrCreate,
    daYkgrDelete,
    daYkgrExecute,
    daYkgrIsDelete,
    daYkgrDraw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_Ykgr = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Ykgr_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daYkgr_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Ykgr_e,
    /* Actor SubMtd */ &daYkgrMethodTable,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ 0,
};

AUDIO_INSTANCES;
