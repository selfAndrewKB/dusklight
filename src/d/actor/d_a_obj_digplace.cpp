/**
 * d_a_obj_digplace.cpp
 * Generic Wolf Digging Spots
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_obj_digplace.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "f_pc/f_pc_name.h"

#if TARGET_PC
#include "dusk/coop/player_query.h"
#include "dusk/coop/retained_interaction_owner.h"

static dusk::coop::PlayerQueryEligibility digWolfEligibility(
    dusk::coop::PlayerSlot, fopAc_ac_c* actor, void*)
{
    dusk::coop::PlayerQueryEligibility eligibility;
    eligibility.eligible = static_cast<daAlink_c*>(actor)->checkWolf();
    if (!eligibility.eligible) {
        eligibility.failureFlags = dusk::coop::PlayerQueryEligibilityFailure_Form;
    }
    return eligibility;
}
#endif

int daObjDigpl_c::create() {
    fopAcM_ct(this, daObjDigpl_c);

    mType = (fopAcM_GetParam(this) >> 8) & 0xF;
    mSwitch = fopAcM_GetParam(this) & 0xFF;

    if (mType == 5) {
        fopAcM_OffStatus(this, fopAcStts_NOEXEC_e);
        mpDigPoints =
            dPath_GetRoomPath((fopAcM_GetParam(this) >> 0x14) & 0xFF, fopAcM_GetRoomNo(this));
    }

    if (mType >= 4) {
        mType = 0;
    }

    if (mSwitch != 0xFF && fopAcM_isSwitch(this, mSwitch)) {
        return cPhs_ERROR_e;
    }

    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_copy(mDoMtx_stack_c::get(), field_0x570);
    fopAcM_SetMtx(this, field_0x570);

    attention_info.position = current.pos;
    eyePos = attention_info.position;
    tevStr.room_no = dComIfGp_roomControl_getStayNo();
    fopAcM_SetMin(this, -550.0f, -250.0f, -550.0f);
    fopAcM_SetMax(this, 550.0f, 250.0f, 550.0f);
    attention_info.distances[fopAc_attn_ETC_e] = 27;

    return cPhs_COMPLEATE_e;
}

static int daObjDigpl_Create(fopAc_ac_c* i_this) {
    return static_cast<daObjDigpl_c*>(i_this)->create();
}

daObjDigpl_c::~daObjDigpl_c() {}

static int daObjDigpl_Delete(daObjDigpl_c* i_this) {
#if TARGET_PC
    dusk::coop::retained_interaction_owner::clearAllRetainedInteractions(i_this);
#endif
    i_this->~daObjDigpl_c();
    return 1;
}

int daObjDigpl_c::execute() {
#if TARGET_PC
    // Co-op: vanilla exposes the actor to a wolf before ALINK checks its own Sense state.
    // Preserve that producer order while choosing which wolf seeds this shared actor this frame.
    const dusk::coop::PlayerQueryResult wolfPlayer = dusk::coop::findNearestPlayerMatching(
        this, "digplace.wolf", digWolfEligibility, nullptr);
    daPy_py_c* player_p = wolfPlayer.found
                              ? static_cast<daPy_py_c*>(wolfPlayer.actor)
                              : daPy_getLinkPlayerActorClass();
#else
    daPy_py_c* player_p = daPy_getLinkPlayerActorClass();
#endif

    if (mDigFlg == 1) {
#if TARGET_PC
        const dusk::coop::retained_interaction_owner::RetainedInteractionState digOwner =
            dusk::coop::retained_interaction_owner::updateRetainedInteraction(
                "digplace.complete", this,
                dusk::coop::retained_interaction_owner::RetainedInteractionScope::Dig);
        if (digOwner.found && digOwner.localPlayer != NULL) {
            player_p = digOwner.localPlayer;
        }
#endif
        if (mSwitch != 0xFF) {
            fopAcM_onSwitch(this, mSwitch);
        }

        cXyz item_pos(current.pos.x, current.pos.y - 30.0f, current.pos.z);

        u8 item_no;
        if (mpDigPoints != NULL) {
            item_no = mpDigPoints->m_points[mCurrentDigPoint].mArg1;
        } else {
            item_no = getItemNum();
        }

        if (mType == 0) {
            fopAcM_createItem(&item_pos, item_no, -1, fopAcM_GetRoomNo(this),
                              &player_p->shape_angle, NULL, 9);
        } else if (mType == 2) {
            fopAcM_createItemFromTable(&item_pos, item_no, -1, fopAcM_GetRoomNo(this),
                                       &player_p->shape_angle, 9, NULL, NULL, NULL, false);
        }

        if (mType == 1) {
            mDigFlg = 2;
        } else if (mpDigPoints != NULL) {
            mUsedDigFlags[mCurrentDigPoint >> 5] |= 1 << (mCurrentDigPoint % 32);
            mDigFlg = 0;
        } else {
            fopAcM_delete(this);
            return 1;
        }
#if TARGET_PC
        dusk::coop::retained_interaction_owner::clearRetainedInteraction(
            "digplace.complete", this,
            dusk::coop::retained_interaction_owner::RetainedInteractionScope::Dig);
#endif
    }

    attention_info.flags &= ~fopAc_AttnFlag_ETC_e;

    if (mpDigPoints != NULL) {
        current.pos.y = player_p->current.pos.y + 100000.0f;
        attention_info.position.y = current.pos.y;
        cXyz point_pos;

        dPnt* point_p = mpDigPoints->m_points;
        f32 dist_to_current_pnt = FLT_MAX;

        for (int i = 0; i < mpDigPoints->m_num; i++, point_p++) {
            if ((mUsedDigFlags[i >> 5] & (1 << (i % 32))) == 0) {
                point_pos.set(point_p->m_position.x, point_p->m_position.y, point_p->m_position.z);

                if (
#if TARGET_PC
                    wolfPlayer.found &&
#endif
                    player_p->current.pos.abs(point_pos) < 1000.0f)
                {
                    dComIfGp_particle_setSimple(0x70F, &point_pos, 255, g_whiteColor, g_whiteColor,
                                                0, 0.0f);
                    dComIfGp_particle_setSimple(0x73D, &point_pos, 255, g_whiteColor, g_whiteColor,
                                                0, 0.0f);
                }

                f32 point_to_plyr_dist = player_p->current.pos.abs2XZ(point_pos);
                if (dist_to_current_pnt > point_to_plyr_dist &&
                    fabsf(point_pos.y - player_p->current.pos.y) < 200.0f)
                {
                    dist_to_current_pnt = point_to_plyr_dist;

                    mCurrentDigPoint = i;
                    current.pos = point_pos;
                    attention_info.position = point_pos;
                    eyePos = point_pos;

                    if (point_p->mArg0 == 0) {
                        mType = 0;
                    } else {
                        mType = 2;
                    }
                }
            }
        }
    }

    if (
#if TARGET_PC
        wolfPlayer.found &&
#else
        daPy_py_c::checkNowWolf() &&
#endif
        mDigFlg == 0)
    {
        if (field_0x56b == 0) {
            int seen_angle = fopAcM_seenActorAngleY(player_p, this);
            f32 dist_to_player = fopAcM_searchActorDistanceXZ2(this, player_p);

            if (seen_angle <= 0x2800 || dist_to_player < 1600.0f) {
                attention_info.flags |= fopAc_AttnFlag_ETC_e;
            }

            if (dist_to_player < 250000.0f &&
                fabsf(current.pos.y - player_p->current.pos.y) < 200.0f)
            {
#if TARGET_PC
                // Co-op: the same eligible ALINK that exposed the dig prompt receives look aim.
                static_cast<daAlink_c*>(player_p)->setLookPosFromOut(&attention_info.position);
#else
                daPy_py_c::setLookPos(&attention_info.position);
#endif
            }
        }

        if (mpDigPoints == NULL && player_p->current.pos.abs(current.pos) < 1000.0f) {
            dComIfGp_particle_setSimple(0x70F, &current.pos, 255, g_whiteColor, g_whiteColor, 0,
                                        0.0f);
            dComIfGp_particle_setSimple(0x73D, &current.pos, 255, g_whiteColor, g_whiteColor, 0,
                                        0.0f);
        }
    }

    return 1;
}

static int daObjDigpl_Execute(daObjDigpl_c* i_this) {
    return i_this->execute();
}

static int daObjDigpl_Draw(daObjDigpl_c* i_this) {
    return 1;
}

static DUSK_CONST actor_method_class l_daObjDigpl_Method = {
    (process_method_func)daObjDigpl_Create,  (process_method_func)daObjDigpl_Delete,
    (process_method_func)daObjDigpl_Execute, (process_method_func)NULL,
    (process_method_func)daObjDigpl_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_Obj_Digpl = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 3,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Digpl_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjDigpl_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Digpl_e,
    /* Actor SubMtd */ &l_daObjDigpl_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e | fopAcStts_NOEXEC_e,
    /* Group        */ fopAc_ENV_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
