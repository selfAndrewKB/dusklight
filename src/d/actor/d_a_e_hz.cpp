/**
 * @file d_a_e_hz.cpp
 *
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "Z2AudioLib/Z2Instances.h"
#include "d/actor/d_a_e_hz.h"
#include "res/Object/E_hz.h"
#include "d/d_camera.h"
#include "f_op/f_op_actor_enemy.h"
#include "d/d_debug_viewer.h"

#if TARGET_PC
#include "d/actor/d_a_alink.h"
#include "dusk/coop/damage_owner.h"
#include "dusk/coop/enemy_targeting.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/retained_interaction_owner.h"
#include "dusk/coop/selected_target_state.h"
#endif

enum daE_HZ_Action {
    /*  0 */ ACTION_WAIT,
    /*  1 */ ACTION_HIDE,
    /*  2 */ ACTION_ATTACK,
    /*  3 */ ACTION_AWAY,
    /*  4 */ ACTION_WIND,
    /*  5 */ ACTION_DAMAGE,
    /*  6 */ ACTION_DEATH,
    /*  7 */ ACTION_CHANCE,
    /*  8 */ ACTION_WIND_CHANCE,
    /*  9 */ ACTION_WIND_WALK,
    /* 10 */ ACTION_WATER_DEATH,
    /* 11 */ ACTION_DEATH_WAIT
};

class daE_HZ_HIO_c : public JORReflexible {
public:
    daE_HZ_HIO_c();
    virtual ~daE_HZ_HIO_c() {}

    void genMessage(JORMContext*);

    /* 0x4 */ s8 enemy_sample;
    /* 0x8 */ f32 basic_size;
    /* 0xC */ u8 pl_detection_radius_display;
    /* 0x10 */ f32 pl_detection_radius_in;
    /* 0x14 */ f32 pl_detection_radius_outside;
    /* 0x18 */ f32 escape_speed;
    /* 0x1C */ f32 piyori_time;
    /* 0x20 */ f32 extension_time;
    /* 0x24 */ f32 stretch_anim_speed;
    /* 0x28 */ f32 retention_time_before_stretch;
    /* 0x2C */ f32 reeling_rotation_speed;
    /* 0x30 */ f32 y_position_wrap;
    /* 0x34 */ f32 camera_on_timer;
    /* 0x38 */ f32 camera_off_timer;
};

daE_HZ_HIO_c::daE_HZ_HIO_c() {
    enemy_sample = -1;
    basic_size = 1.0f;
    pl_detection_radius_display = 0;
    pl_detection_radius_in = 700.0f;
    pl_detection_radius_outside = 1200.0f;
    escape_speed = 20.0f;
    piyori_time = 160.0f;
    reeling_rotation_speed = 5000.0f;
    y_position_wrap = 400.0f;
    extension_time = 15.0f;
    stretch_anim_speed = 0.7f;
    retention_time_before_stretch = 7.0f;
    camera_on_timer = 1.0f;
    camera_off_timer = 50.0f;
}

#if DEBUG
void daE_HZ_HIO_c::genMessage(JORMContext* ctx) {
    ctx->genLabel("  敵サンプル", 0x80000001);
    ctx->genSlider("基本サイズ", &basic_size, 0.0f, 5.0f);
    ctx->genCheckBox("PL発見半径表示", &pl_detection_radius_display, 0x1);
    ctx->genSlider("PL発見半径（内）", &pl_detection_radius_in, 0.0f, 5000.0f);
    ctx->genSlider("PL発見半径（外）", &pl_detection_radius_outside, 0.0f, 5000.0f);
    ctx->genSlider("逃げ速度", &escape_speed, 0.0f, 200.0f);
    ctx->genSlider("ぴより時間", &piyori_time, 0.0f, 500.0f);
    ctx->genSlider("巻き込み回転速度", &reeling_rotation_speed, 0.0f, 10000.0f);
    ctx->genSlider("巻き込みＹ位置", &y_position_wrap, 0.0f, 1000.0f);
    ctx->genSlider("伸び溜め時間", &extension_time, 0.0f, 100.0f);
    ctx->genSlider("伸びアニメ速度", &stretch_anim_speed, 0.0f, 1.0f);
    ctx->genSlider("伸び前の溜め時間", &retention_time_before_stretch, 0.0f, 100.0f);
    ctx->genLabel("カメラ用タイマー", 0x80000001);
    ctx->genSlider("オンタイマー", &camera_on_timer, 0.0f, 100.0f);
    ctx->genSlider("オフタイマー", &camera_off_timer, 0.0f, 100.0f);
}
#endif

void daE_HZ_c::setBck(int i_index, u8 i_attr, f32 i_morf, f32 i_rate) {
    mpMorfSO->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_HZ", i_index), i_attr, i_morf, i_rate, 0.0f, -1.0f);
}

bool daE_HZ_c::checkBck(int i_index) {
    if (mpMorfSO->getAnm() == (J3DAnmTransform*)dComIfG_getObjectRes("E_HZ", i_index)) {
        return true;
    }

    return false;
}

static u8 hio_set;

static daE_HZ_HIO_c l_HIO;

#if TARGET_PC
namespace {

struct CoOpHzTargetState {
    dusk::coop::selected_target_state::SelectedTargetState state;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
};

struct CoOpHzAttackCandidateData {
    daE_HZ_c* enemy = NULL;
};

struct CoOpHzExactPlayerData {
    fopAc_ac_c* player = NULL;
};

static dusk::coop::PlayerQueryEligibility coOpHzAttackCandidate(
    dusk::coop::PlayerSlot slot, fopAc_ac_c* actor, void* userData) {
    dusk::coop::PlayerQueryEligibility eligibility;
    CoOpHzAttackCandidateData* data = static_cast<CoOpHzAttackCandidateData*>(userData);
    if (actor == NULL || data == NULL || data->enemy == NULL) {
        eligibility.eligible = false;
        eligibility.failureFlags = dusk::coop::PlayerQueryEligibilityFailure_Status;
        return eligibility;
    }

    const dusk::coop::selected_target_state::SelectedTargetState state =
        dusk::coop::selected_target_state::stateForSlot(slot, actor);
    if (actor->current.pos.absXZ(data->enemy->current.pos) >= 100.0f) {
        eligibility.eligible = false;
        eligibility.failureFlags |= dusk::coop::PlayerQueryEligibilityFailure_Range;
    }
    if (std::abs(actor->current.pos.y - data->enemy->current.pos.y) >= 110.0f) {
        eligibility.eligible = false;
        eligibility.failureFlags |= dusk::coop::PlayerQueryEligibilityFailure_Vertical;
    }
    if (!state.available || state.player == NULL || state.player->checkBootsOrArmorHeavy()) {
        eligibility.eligible = false;
        eligibility.failureFlags |= dusk::coop::PlayerQueryEligibilityFailure_Status;
    }
    return eligibility;
}

static dusk::coop::PlayerQueryEligibility coOpHzExactPlayer(
    dusk::coop::PlayerSlot, fopAc_ac_c* actor, void* userData) {
    dusk::coop::PlayerQueryEligibility eligibility;
    CoOpHzExactPlayerData* data = static_cast<CoOpHzExactPlayerData*>(userData);
    if (data == NULL || actor != data->player) {
        eligibility.eligible = false;
        eligibility.failureFlags = dusk::coop::PlayerQueryEligibilityFailure_Status;
    }
    return eligibility;
}

// Co-op: Tile Worm awareness, launch setup, and active movement share one Combat target. Item
// reactions and the retained launch victim remain separate ownership questions.
static bool coOpSelectHzTargetState(
    daE_HZ_c* i_this, const char* label, bool committed, dusk::coop::EnemyTargetMode mode,
    CoOpHzTargetState* out, dusk::coop::PlayerQueryPredicate predicate = NULL,
    void* predicateData = NULL) {
    dusk::coop::EnemyTargetContext context;
    context.observer = i_this;
    context.scope = dusk::coop::EnemyTargetScope::Combat;
    context.mode = mode;
    context.label = label;
    context.candidatePredicate = predicate;
    context.candidatePredicateData = predicateData;
    context.committed = committed;

    const dusk::coop::EnemyTargetResult target = dusk::coop::selectEnemyTarget(context);
    const dusk::coop::selected_target_state::SelectedTargetState targetState =
        dusk::coop::selected_target_state::stateForEnemyTarget(target);
    dusk::coop::selected_target_state::recordSelectedTargetState(
        i_this, label, targetState,
        targetState.available
            ? dusk::coop::selected_target_state::SelectedTargetStateReason::EnemyTarget
            : dusk::coop::selected_target_state::SelectedTargetStateReason::InvalidTarget);
    if (!targetState.available) {
        return false;
    }

    if (out != NULL) {
        out->state = targetState;
        out->distance = target.distance;
        out->distanceXZ = target.distanceXZ;
        out->angleY = target.angleY;
    }
    return true;
}

static void coOpHzSelectExactPlayer(daE_HZ_c* i_this, fopAc_ac_c* player,
                                    const char* label) {
    if (player == NULL) {
        return;
    }
    CoOpHzExactPlayerData data;
    data.player = player;
    // Co-op: an exact hit/interaction owner must replace stale combat retention before commit.
    coOpSelectHzTargetState(i_this, label, false, dusk::coop::EnemyTargetMode::ImmediateAcquire,
                            NULL, coOpHzExactPlayer, &data);
}

static bool coOpHzAnyHookshotActive() {
    bool active = false;
    dusk::coop::forEachActivePlayer([&](dusk::coop::PlayerSlot slot, fopAc_ac_c* actor) {
        if (active) {
            return;
        }
        const dusk::coop::selected_target_state::SelectedTargetState state =
            dusk::coop::selected_target_state::stateForSlot(slot, actor);
        active = state.available && state.status0_0x4000;
    });
    return active;
}

static bool coOpHzAnyToolKeepsHidden(daE_HZ_c* i_this) {
    bool active = false;
    dusk::coop::forEachActivePlayer([&](dusk::coop::PlayerSlot slot, fopAc_ac_c* actor) {
        if (active) {
            return;
        }
        const dusk::coop::selected_target_state::SelectedTargetState state =
            dusk::coop::selected_target_state::stateForSlot(slot, actor);
        if (!state.available || state.player == NULL) {
            return;
        }

        cXyz* hookTop = state.player->getHookshotTopPos();
        if (state.status0_0x4000 && hookTop != NULL && hookTop->abs(i_this->current.pos) < 500.0f) {
            active = true;
            return;
        }

        cXyz* ironBall = state.player->getIronBallCenterPos();
        if (ironBall != NULL && ironBall->absXZ(i_this->current.pos) < 400.0f) {
            active = true;
        }
    });
    return active;
}

static dusk::coop::retained_interaction_owner::RetainedInteractionState coOpHzThrowOwner(
    daE_HZ_c* i_this, const char* label) {
    return dusk::coop::retained_interaction_owner::updateRetainedInteraction(
        label, i_this, dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach);
}

static daPy_py_c* coOpHzThrowPlayer(daE_HZ_c* i_this, const char* label) {
    const dusk::coop::retained_interaction_owner::RetainedInteractionState owner =
        coOpHzThrowOwner(i_this, label);
    return owner.found && owner.localPlayer != NULL ? owner.localPlayer
                                                    : daPy_getPlayerActorClass();
}

static dCamera_c* coOpHzThrowCamera(daE_HZ_c* i_this, const char* label) {
    const dusk::coop::retained_interaction_owner::RetainedInteractionState owner =
        coOpHzThrowOwner(i_this, label);
    const int slot = owner.found ? static_cast<int>(owner.slot) : 0;
    camera_process_class* camera =
        dComIfGp_getCamera(dComIfGp_getPlayerCameraID(slot));
    if (camera != NULL) {
        return &camera->mCamera;
    }
    return owner.found ? NULL : dCam_getBody();
}

static void coOpClearHzThrow(daE_HZ_c* i_this, const char* label) {
    dCamera_c* camera = coOpHzThrowCamera(i_this, label);
    if (camera != NULL && camera->GetForceLockOnActor() == i_this) {
        camera->ForceLockOff(i_this);
    }
    dusk::coop::retained_interaction_owner::clearRetainedInteraction(
        label, i_this, dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach);
}

static daBoomerang_c* coOpHzBoomerangActor(daE_HZ_c* i_this) {
    const dusk::coop::EnemyTargetResult target =
        dusk::coop::getEnemyTarget(i_this, dusk::coop::EnemyTargetScope::Combat);
    const dusk::coop::selected_target_state::SelectedTargetState state =
        dusk::coop::selected_target_state::stateForEnemyTarget(target);
    if (state.available && state.actor != NULL &&
        fopAcM_GetName(state.actor) == fpcNm_ALINK_e && !state.player->checkWolf())
    {
        fopAc_ac_c* boomerang = static_cast<daAlink_c*>(state.actor)->getBoomerangActor();
        if (boomerang != NULL && fopAcM_GetName(boomerang) == fpcNm_BOOMERANG_e) {
            return static_cast<daBoomerang_c*>(boomerang);
        }
    }
    return NULL;
}

static f32 coOpHzDistanceToTarget(daE_HZ_c* i_this, fopAc_ac_c* observer) {
    const dusk::coop::EnemyTargetResult target =
        dusk::coop::getEnemyTarget(i_this, dusk::coop::EnemyTargetScope::Combat);
    if (target.found && target.localActor != NULL) {
        return fopAcM_searchActorDistance(observer, target.localActor);
    }
    return fopAcM_searchPlayerDistance(observer);
}

static bool coOpHzRightTurnCut(int cutType) {
    return cutType == daPy_py_c::CUT_TYPE_TURN_RIGHT ||
           cutType == daPy_py_c::CUT_TYPE_UNK_9 ||
           cutType == daPy_py_c::CUT_TYPE_HORSE_TURN ||
           cutType == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT;
}

}  // namespace
#endif

int daE_HZ_c::draw() {
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(mpModel2, &tevStr);
    mDoExt_modelUpdateDL(mpModel2);

    if (field_0x6e8 != 0) {
        return 1;
    }
    
    J3DMaterial* material;
    J3DModel* morfModel = mpMorfSO->getModel();
    g_env_light.setLightTevColorType_MAJI(morfModel, &tevStr);

    if (mReadyChangeColor != 0) {
        J3DModelData* morfModelData = morfModel->getModelData();
        for (u16 i = 0; i < morfModelData->getMaterialNum(); i++) {
            material = morfModelData->getMaterialNodePointer(i);
            material->getTevColor(0)->r = mColor;
            material->getTevColor(0)->g = mColor;
            material->getTevColor(0)->b = mColor;
        }
    }

    mpMorfSO->entryDL();
    g_env_light.settingTevStruct(0x10, &current.pos, &mTevStr);
    g_env_light.setLightTevColorType_MAJI(mpModel, &mTevStr);
    mDoExt_modelUpdateDL(mpModel);

    cXyz modified_pos;
    modified_pos.set(current.pos.x, current.pos.y + 100.0f, current.pos.z);

    if (!mDisableShadow) {
        mShadowKey = dComIfGd_setShadow(mShadowKey, 1, morfModel, &modified_pos, nREG_F(19) + 800.0f, 0.0f,
                                        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd,
                                        &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }

    #if DEBUG
    if (l_HIO.pl_detection_radius_display) {
        cXyz pos(home.pos);
        pos.y += 100.0f;
        dDbVw_drawCircleOpa(pos, l_HIO.pl_detection_radius_in, (GXColor){0xC8, 0xC8, 0x00, 0xFF}, 1, 12);
        dDbVw_drawCircleOpa(pos, l_HIO.pl_detection_radius_outside, (GXColor){0x00, 0xC8, 0x00, 0xFF}, 1, 12);

        if (field_0x6e4) {
            pos = current.pos;
            pos.y += mAcchCir.GetWallH();
            dDbVw_drawCircleOpa(pos, mAcchCir.GetWallR(), (GXColor){0x00, 0x00, 0xC8, 0xFF}, 1, 12);
        }
    }
    #endif
    
    return 1;
}

static int daE_HZ_Draw(daE_HZ_c* i_this) {
    return i_this->draw();
}

static fopAc_ac_c* m_near_bomb;

static fopAc_ac_c* m_near_carry;

static fopAc_ac_c* m_near_weapon;

static void* s_obj_sub(void* i_actor, void* i_data) {
    f32 actor_dist;

    UNUSED(i_data);

    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && !fopAcM_checkCarryNow((fopAc_ac_c*)i_actor))
        {
            actor_dist = fopAcM_searchActorDistance((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_data);

            if (actor_dist < 100.0f && !fopAcM_GetSpeedF((fopAc_ac_c*)i_actor) &&
                !fopAcM_GetSpeed((fopAc_ac_c*)i_actor).y)
            {
                if (fopAcM_GetName(i_actor) == fpcNm_Obj_Carry_e) {
                    m_near_carry = (fopAc_ac_c*)i_actor;
                } else if (fopAcM_GetName(i_actor) == fpcNm_NBOMB_e) {
                    m_near_bomb = (fopAc_ac_c*)i_actor;
                }
            }

            if (actor_dist < 500.0f && (fopAcM_GetName(i_actor) == fpcNm_BOOMERANG_e ||
                                        fopAcM_GetName(i_actor) == fpcNm_NBOMB_e))
            {
                m_near_weapon = static_cast<fopAc_ac_c*>(i_actor);
            }
            if (actor_dist < 700.0f && fopAcM_GetName(i_actor) == fpcNm_ARROW_e &&
                fopAcM_GetSpeedF((fopAc_ac_c*)i_actor))
            {
                m_near_weapon = static_cast<fopAc_ac_c*>(i_actor);
            }
        }
    }
    return NULL;
}

void daE_HZ_c::setActionMode(int i_action) {
    if (mAction != i_action || i_action == 5) {
        if (mReadyNewAction) {
            dComIfGp_event_reset();
            mReadyNewAction = false;
            OS_REPORT("setActionMode %d %d \n", mAction, i_action);
        }
        mDisableShadow = false;
        mPrevAction = mAction;
        mAction = i_action;
        mMode = 0;
        current.angle.y = shape_angle.y;
    }
}

bool daE_HZ_c::checkHideStart() {
#if !TARGET_PC
    daPy_py_c* player = daPy_getPlayerActorClass();
#endif

    if (mAttackCooldownTimer != 0) {
        return 1;
    }

    if (mPlayerDist < l_HIO.pl_detection_radius_in ||
        mPlayerDist > l_HIO.pl_detection_radius_outside)
    {
        return 1;
    }

#if TARGET_PC
    if (coOpHzAnyToolKeepsHidden(this)) {
        return 1;
    }
#else
    if (dComIfGp_checkPlayerStatus0(0, 0x4000) && player->getHookshotTopPos() != NULL) {
        if (player->getHookshotTopPos()->abs(current.pos) < 500.0f) {
            return 1;
        }
    }
#endif

    m_near_bomb = m_near_carry = m_near_weapon = NULL;

    fpcM_Search(s_obj_sub, this);

    if (m_near_bomb != NULL || m_near_carry != NULL || m_near_weapon != NULL) {
        return 1;
    }

#if !TARGET_PC
    if (player->getIronBallCenterPos() != NULL) {
        if (player->getIronBallCenterPos()->absXZ(current.pos) < 400.0f) {
            return 1;
        }
    }
#endif

    return 0;
}

bool daE_HZ_c::checkAttackStart() {
    field_0x6e9 = 0;

#if TARGET_PC
    CoOpHzAttackCandidateData candidateData;
    candidateData.enemy = this;
    CoOpHzTargetState target;
    // Co-op: filter the launch candidate before committing so a heavy P1 cannot mask P2.
    if (coOpSelectHzTargetState(this, "e_hz.attack_start", false,
                                dusk::coop::EnemyTargetMode::ImmediateAcquire, &target,
                                coOpHzAttackCandidate, &candidateData))
    {
        // Co-op: retain the exact player standing on the tile before the native event request.
        dusk::coop::retained_interaction_owner::beginRetainedInteraction(
            "e_hz.throw_begin", this,
            dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach,
            target.state.actor,
            dusk::coop::retained_interaction_owner::RetainedInteractionReason::EnemyTarget);
        field_0x6e9 |= (u8)1;
        mPlayerDist = target.distance;
        mPlayerAngleY = target.angleY;
    } else {
        bool heavyPlayerNearby = false;
        dusk::coop::forEachActivePlayer(
            [&](dusk::coop::PlayerSlot, fopAc_ac_c* actor) {
                if (heavyPlayerNearby || actor == NULL) {
                    return;
                }
                daPy_py_c* player = static_cast<daPy_py_c*>(actor);
                heavyPlayerNearby =
                    actor->current.pos.absXZ(current.pos) < 100.0f &&
                    std::abs(actor->current.pos.y - current.pos.y) < 110.0f &&
                    player->checkBootsOrArmorHeavy();
            });
        if (heavyPlayerNearby) {
            if (mAttackStartTimer == 0) {
                mMode = 2;
                current.pos.y += 2.0f;
                mAttackStartTimer = cM_rndF(10.0f) + 20.0f;
            }
            return false;
        }
    }
#else
    if (fopAcM_searchPlayerDistanceXZ(this) < 100.0f &&
        std::abs(fopAcM_searchPlayerDistanceY(this)) < 110.0f)
    {
        if (!daPy_getPlayerActorClass()->checkBootsOrArmorHeavy()) {
            field_0x6e9 |= (u8)1;
        } else {
            if (mAttackStartTimer == 0) {
                mMode = 2;
                current.pos.y += 2.0f;
                mAttackStartTimer = cM_rndF(10.0f) + 20.0f;
            }
            return false;
        }
    }
#endif

    m_near_bomb = m_near_carry = m_near_weapon = NULL;

    fpcM_Search(s_obj_sub, this);
    cXyz unused;

    if (m_near_bomb != NULL && !((daNbomb_c*)m_near_bomb)->checkStateExplode()) {
        field_0x6e9 |= (u8)4;
        mpBombActor = (daNbomb_c*)m_near_bomb;
    }

    if (m_near_carry != NULL) {
        field_0x6e9 |= (u8)2;
        mpCarryActor = (daObjCarry_c*)m_near_carry;
    }

    if (field_0x6e9) {
        return true;
    }

    return false;
}

bool daE_HZ_c::checkArrowCharge() {
#if TARGET_PC
    bool charged = false;
    dusk::coop::forEachActivePlayer([&](dusk::coop::PlayerSlot, fopAc_ac_c* actor) {
        if (!charged && actor != NULL) {
            charged = static_cast<daPy_py_c*>(actor)->checkArrowChargeEnd();
        }
    });
    return charged;
#else
    if (daPy_getPlayerActorClass()->checkArrowChargeEnd()) {
        return true;
    }
    return false;
#endif
}

void daE_HZ_c::setTgSetBit(int i_setBit) {
    if (i_setBit) {
        mSpheres[1].OnTgSetBit();
        mSpheres[2].OnTgSetBit();
        mSpheres[3].OnTgSetBit();
    } else {
        mSpheres[1].OffTgSetBit();
        mSpheres[2].OffTgSetBit();
        mSpheres[3].OffTgSetBit();
    }
}

bool daE_HZ_c::isWait() {
    if (mAction == ACTION_WAIT || mAction == ACTION_HIDE) {
        return true;
    }

    if (mAction == ACTION_DEATH_WAIT && mMode == ACTION_WIND) {
        return true;
    }
    return false;
}

void daE_HZ_c::checkFall() {
    if (mAction == ACTION_DEATH_WAIT || mAction == ACTION_WATER_DEATH) {
        return;
    }

    if (mObjAcch.ChkGroundHit()) {
        int acch_ground_code = dComIfG_Bgsp().GetGroundCode(mObjAcch.m_gnd);
        // FAKEMATCH???
        #if DEBUG
        if (acch_ground_code != 4 && acch_ground_code != 10 && acch_ground_code != 5) {
            return;
        }
        #else
        if (acch_ground_code != 10 && (acch_ground_code - 4 > 1u)) {
            return;
        }
        #endif
    }

    dBgS_GndChk gnd_chk;
    cXyz modified_pos(current.pos);
    modified_pos.y += 300.0f;
    gnd_chk.SetPos(&modified_pos);

    f32 ground_cross = dComIfG_Bgsp().GroundCross(&gnd_chk);
    if (speed.y < 0.0f && ground_cross != -G_CM3D_F_INF) {
        s32 gnd_chk_ground_code = dComIfG_Bgsp().GetGroundCode(gnd_chk);
        if ((gnd_chk_ground_code == 4 || gnd_chk_ground_code == 10 || gnd_chk_ground_code == 5) &&
            ground_cross + 500.0f > current.pos.y)
        {
            cXyz pos;
            mDoMtx_stack_c::copy(mpMorfSO->getModel()->getAnmMtx(0x12));
            mDoMtx_stack_c::multVecZero(&pos);
            fopAcM_createDisappear(this, &pos, 10, 0, 5);
            setActionMode(0xB);
        }
    }
}

void daE_HZ_c::setCloseSmokeEffect() {
    dBgS_GndChk gnd_chk;

    cXyz modified_pos(current.pos);
    modified_pos.y += 100.0f;

    gnd_chk.SetPos(&modified_pos);
    f32 groundCross = dComIfG_Bgsp().GroundCross(&gnd_chk);
    if (groundCross != -G_CM3D_F_INF) {
        dComIfGp_particle_setPolyColor(dPa_RM(ID_ZM_S_HAZIKIPLATESMOKE00), gnd_chk, &mSmokeEffectPosition, &tevStr,
                                       &shape_angle, NULL, 0, NULL, -1, NULL);
    }

    mSound.startCreatureSound(Z2SE_EN_HZ_CLOSE, 0, -1);
}

void daE_HZ_c::executeWait() {
    f32 target;

    field_0x566 = 0;
    mSetModelAnmMtx = true;

    switch (mMode) {
    case 0:
        mSpheres[0].OnTgSetBit();
        mSpheres[0].SetTgType(0x10040);
        setBck(0x10, 2, 0.0f, 1.0f);
        shape_angle.y = (s16)(mPlayerAngleY + 0x2000) & 0xC000;
        mMode = 1;
        field_0x6e4 = 0;
        field_0x6b4 = 0;
        /* fallthrough */
    case 1:
        if (!cLib_chaseF(&current.pos.y, home.pos.y + 30.0f, 2.0f)) {
            break;
        } else {
            mMode = 2;
            field_0x6b2 = shape_angle.y = (s16)(shape_angle.y + 0x2000) & 0xC000;
            field_0x6cc = 0;
            field_0x6b6 = 0;
        }
        break;

    case 2:
        shape_angle.y += field_0x6b6;
        switch (field_0x6cc) {
        case 0:
            if ((s16)(field_0x6b2 - 0x1800) < shape_angle.y) {
                cLib_chaseAngleS(&field_0x6b6, -0x200, 0x80);
            } else if (cLib_chaseAngleS(&field_0x6b6, 0, 0x80)) {
                field_0x6cc = 1;
                mWaitTimer = 10;
            }
            break;

        case 2:
            if ((s16)(field_0x6b2 + 0x1800) > shape_angle.y) {
                cLib_chaseAngleS(&field_0x6b6, 0x200, 0x80);
            } else if (cLib_chaseAngleS(&field_0x6b6, 0, 0x80)) {
                field_0x6cc = 3;
                mWaitTimer = 10;
            }
            break;

        case 3:
        case 1:
            if (mWaitTimer == 0) {
                field_0x6cc++;
            }
            break;

        case 4:
            cLib_chaseAngleS(&field_0x6b6, -0x200, 0x80);
            if (cLib_distanceAngleS(shape_angle.y, field_0x6b2) >= 0x500) {
                break;
            }
            mWaitTimer = 10;
            field_0x6cc = 5;
            return;

        case 5:
            cLib_chaseAngleS(&field_0x6b6, 0, 0x80);
            if (mWaitTimer == 0) {
                if (checkArrowCharge()) {
                    mMode = 4;
                } else {
                    mMode = 3;
                }
            }
        }
        
        if (field_0x6cc >= 2 && field_0x6cc <= 4 &&
            cLib_distanceAngleS(shape_angle.y, mPlayerAngleY) < 0x800)
        {
            mWaitTimer = 0;
            field_0x6cc = 5;
            break;
        }
        break;

    case 3:
    case 4:
        target = home.pos.y + 30.0f;
        if (cLib_chaseF(&current.pos.y, target, 5.0f)) {
            ANGLE_ADD(field_0x6b4, 0x800);
            current.pos.y += cM_ssin(field_0x6b4) * 1.5f;
            cLib_chaseAngleS(&shape_angle.y, mPlayerAngleY, 0x400);
        }
    }

    if (checkBck(0x10) && (mpMorfSO->checkFrame(0.0f) || mpMorfSO->checkFrame(30.0f))) {
        mSound.startCreatureVoice(Z2SE_EN_HZ_V_WAIT, -1);
    }
    if (checkHideStart()) {
        setActionMode(ACTION_HIDE);
    }
}

f32 daE_HZ_c::getHideSpeed() {
    f32 ret;
    if (field_0x6cc < 3) {
        if (home.pos.y != current.pos.y) {
            ret = 0.0f;
            current.pos.y += 10.0f;
        }
        field_0x6cc++;
    } else {
        ret = 15.0f;
        if (current.pos.y - home.pos.y > 50.0f) {
            ret = 50.0f;
        }
    }
    return ret;
}

void daE_HZ_c::executeHide() {
    f32 step;

    field_0x566 = 0;
    mSetModelAnmMtx = true;

    switch (mMode) {
    case 0:
        mSpheres[0].OnTgSetBit();
        mSpheres[0].SetTgType(0x10000);
        field_0x6e4 = 0;
        setTgSetBit(0);
        mSpheres[1].OffTgShield();
        mSpheres[2].OffTgShield();
        mSpheres[1].SetTgHitMark(CcG_Tg_UNK_MARK_0);
        mSpheres[2].SetTgHitMark(CcG_Tg_UNK_MARK_0);
        mWaitTimer = 0;
        field_0x6cc = 0;
        if (home.pos.y == current.pos.y) {
            mMode = 3;
            setBck(0xD, 0, 3.0f, 0.0f);
            shape_angle.y = home.angle.y;
        } else {
            mMode = 1;
            setBck(0x10, 2, 0.0f, 1.0f);
        }
        field_0x6b2 = (s16)(shape_angle.y + 0x2000) & 0xC000;
        speed.y = speedF = 0.0f;
        gravity = 0.0f;
        break;

    case 1:
        s16 chaseAngle;
        if (mPlayerDist > l_HIO.pl_detection_radius_outside) {
            chaseAngle = 0x200;
        } else {
            chaseAngle = 0x800;
            step = getHideSpeed();
            if (cLib_chaseF(&current.pos.y, home.pos.y, step)) {
                shape_angle.y = field_0x6b2;
            }
        }
        if (cLib_chaseAngleS(&shape_angle.y, field_0x6b2, chaseAngle)) {
            mMode = 2;
        }
        break;

    case 2:
        if (mPlayerDist > l_HIO.pl_detection_radius_outside) {
            step = 5.0f;
        } else {
            step = getHideSpeed();
        }
        
        if (cLib_chaseF(&current.pos.y, home.pos.y, step) && mWaitTimer == 0) {
            mMode = 3;
            setBck(0xD, 0, 3.0f, 0.0f);
            setCloseSmokeEffect();
        }
        break;

    case 3:
        if (checkAttackStart()) {
            if (field_0x6e9 & 1) {
                mMode = 4;
            } else {
                mMode = 5;
                mRetentionBeforeStretchTimer = l_HIO.retention_time_before_stretch;
            }
            shape_angle.y = (s16)(mPlayerAngleY + 0x2000) & 0xC000;
            break;
        }
        if (!checkHideStart() &&
#if TARGET_PC
            !coOpHzAnyHookshotActive()
#else
            !dComIfGp_checkPlayerStatus0(0, 0x4000)
#endif
        ) {
            setActionMode(ACTION_WAIT);
        }
        break;

    case 4:
        if (!eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(this, 2, 0, 0);
            eventInfo.onCondition(dEvtCnd_CANDEMO_e);
        } else {
            mReadyNewAction = true;
            mMode = 5;
            mRetentionBeforeStretchTimer = l_HIO.retention_time_before_stretch;
        }
        break;

    case 5:
        cLib_chaseF(&current.pos.y, home.pos.y + 50.0f, 10.0f);

        if (mRetentionBeforeStretchTimer == 2) {
            if (mReadyNewAction) {
#if TARGET_PC
                daPy_py_c* throwPlayer = coOpHzThrowPlayer(this, "e_hz.throw_position");
#else
                daPy_py_c* throwPlayer = daPy_getPlayerActorClass();
#endif
                cXyz position(current.pos.x, throwPlayer->current.pos.y, current.pos.z);

                throwPlayer->setPlayerPosAndAngle(&position, throwPlayer->shape_angle.y, 0);
                dComIfGp_event_reset();
                mReadyNewAction = false;
            }
        } else if (mRetentionBeforeStretchTimer == 1) {
            mSetModelAnmMtx = false;

            if (field_0x6e9 & 1) {
#if TARGET_PC
                coOpHzThrowPlayer(this, "e_hz.throw_damage")
                    ->setThrowDamage(shape_angle.y, 35.0f, 73.0f, 1, 0, 0);
#else
                daPy_getPlayerActorClass()->setThrowDamage(shape_angle.y, 35.0f, 73.0f, 1, 0,
                                                           0);
#endif
                mCameraOnTimer = l_HIO.camera_on_timer + 1.0f;
            } else if (field_0x6e9 & 2) {
                if (mpCarryActor != NULL) {
                    fopAcM_SetSpeedF(mpCarryActor, 35.0f);
                    speed = fopAcM_GetSpeed(mpCarryActor);
                    fopAcM_SetSpeed(mpCarryActor, speed.x, 73.0f, speed.y);
                    mpCarryActor->shape_angle.y = shape_angle.y;
                    mpCarryActor->current.angle.y = shape_angle.y;
                    mpCarryActor->mode_init_drop(0);
                    mpCarryActor = NULL;
                }
            } else if (field_0x6e9 & 4) {
                if (mpBombActor != NULL && !mpBombActor->checkStateExplode()) {
                    fopAcM_SetSpeedF(mpBombActor, 35.0f);
                    speed = fopAcM_GetSpeed(mpBombActor);
                    fopAcM_SetSpeed(mpBombActor, speed.x, 73.0f, speed.y);
                    mpBombActor->shape_angle.y = shape_angle.y;
                    mpBombActor->current.angle.y = shape_angle.y;
                    mpBombActor = NULL;
                }
            }
            field_0x6e9 = 0;
            setActionMode(ACTION_ATTACK);
        }
    }
}

void daE_HZ_c::executeAttack() {
    static u16 d_HZ_JUMP_EFFECT_ID[2] = {
        dPa_RM(ID_ZM_S_HAZIKIAPPEAR00),
        dPa_RM(ID_ZM_S_HAZIKIAPPEAR01),
    };
    cXyz position;
    switch (mMode) {
    case 0:
        current.pos = home.pos;
        setTgSetBit(1);
        mSpheres[1].OnTgShield();
        mSpheres[2].OnTgShield();
        mSpheres[1].SetTgHitMark(CcG_Tg_UNK_MARK_2);
        mSpheres[2].SetTgHitMark(CcG_Tg_UNK_MARK_2);
        mMode = 1;
        setBck(0xD, 0, 3.0f, l_HIO.stretch_anim_speed);
        mSound.startCreatureVoice(Z2SE_EN_HZ_V_JUMP, -1);
        mSound.startCreatureSound(Z2SE_EN_HZ_JUMP, 0, -1);
        break;

    case 1:
        MtxP anmMtx = mpMorfSO->getModel()->getAnmMtx(1);
        position.set(anmMtx[0][3], anmMtx[1][3], anmMtx[2][3]);

        for (int i = 0; i < 2; i++) {
            mJumpEffects[i] =
                dComIfGp_particle_set(mJumpEffects[i], d_HZ_JUMP_EFFECT_ID[i], &position, &tevStr,
                                      &shape_angle, NULL, 0xff, NULL, -1, NULL, NULL, NULL);
        }

        if (mpMorfSO->checkFrame(8.0f)) {
            mpMorfSO->setPlaySpeed(1.0f);
        } else if (mpMorfSO->checkFrame(38.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_LAUGH, -1);
        }

#if TARGET_PC
        if (!coOpHzThrowPlayer(this, "e_hz.attack_throw")->checkThrowDamage()) {
#else
        if (!daPy_getPlayerActorClass()->checkThrowDamage()) {
#endif
            if (mpMorfSO->getFrame() > 60.0f && mpMorfSO->getFrame() < 100.0f && checkHideStart()) {
                mpMorfSO->setFrame(100.0f);
            }
        }

        if (mpMorfSO->isStop()) {
            setCloseSmokeEffect();
            mAttackCooldownTimer = 20;
            setActionMode(ACTION_HIDE);
        }

        break;
    }

#if TARGET_PC
    dCamera_c* throwCamera = coOpHzThrowCamera(this, "e_hz.attack_camera");
    if (!coOpHzThrowPlayer(this, "e_hz.attack_camera")->checkThrowDamage() &&
        throwCamera != NULL && throwCamera->GetForceLockOnActor() == this &&
        mCameraOffTimer == 0)
#else
    if (!daPy_getPlayerActorClass()->checkThrowDamage() &&
        dCam_getBody()->GetForceLockOnActor() == this && mCameraOffTimer == 0)
#endif
    {
        mCameraOffTimer = l_HIO.camera_off_timer + 1.0f;
    }
}

void daE_HZ_c::initBackWalk() {
    cXyz position;
    cXyz start;
    cXyz end;

    speedF = speed.y = 0.0f;
    setTgSetBit(1);
    mSpheres[0].OnTgSetBit();
    mSpheres[0].SetTgType(0x10000);

    if ((current.pos - home.pos).absXZ() < 300.0f) {
        setBck(0x11, 2, 5.0f, 1.5f);
        mMode = 3;
    } else {
        setBck(0x11, 2, 5.0f, 1.5f);
        mMode = 2;

        s16 targetAngleY = cLib_targetAngleY(&home.pos, &current.pos);
        s16 modifiedTargetAngleY = (targetAngleY + 0x2000) & 0xC000;
        position.set(0.0f, 0.0f, 300.0f);
        cLib_offsetPos(&field_0x678, &home.pos, modifiedTargetAngleY, &position);
        dBgS_LinChk linChk;
        position.set(0.0f, 100.0f, 0.0f);
        start = current.pos + position;
        end = field_0x678 + position;
        linChk.Set(&start, &end, NULL);

        if (dComIfG_Bgsp().LineCross(&linChk)) {
            if (modifiedTargetAngleY & 0x4000) {
                targetAngleY = (s16)(targetAngleY + 0x4000 & 0x8000);
            } else {
                targetAngleY = (s16)((targetAngleY & 0x8000) + 0x4000);
            }
            cLib_offsetPos(&field_0x678, &home.pos, targetAngleY, &position);
        }
    }
}

void daE_HZ_c::executeAway() {
    f32 diff;
    switch (mMode) {
    case 0:
        gravity = 0.0f;
        speed.y = 0.0f;
        mSpheres[0].OffTgSetBit();
        shape_angle.x = shape_angle.z = 0;
        setBck(0xF, 0, 10.0f, 1.0f);
        field_0x6e4 = 1;
        mMode = 1;
        field_0x6cc = 0;
        field_0x6ea = 0;
        break;

    case 1:
        if (mpMorfSO->checkFrame(4.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_STAND, -1);
        } else if (mpMorfSO->checkFrame(20.0f)) {
            mSound.startCreatureSound(Z2SE_EN_HZ_SWING, 0, -1);
        }

        speedF = 0.0f;
        if (field_0x6ea == 0) {
            if (mpMorfSO->getFrame() >= 35.0f) {
                gravity = -5.0f;
                if (mObjAcch.ChkGroundHit()) {
                    mSound.startCreatureSound(Z2SE_CM_BODYFALL_M, 0, -1);
                    field_0x6ea++;
                } else {
                    if (speed.y > -20.0f) {
                        speed.y = -20.0f;
                    }
                    mpMorfSO->setFrame(35.0f);
                }
            } else if (mpMorfSO->getFrame() >= 23.0f) {
                speedF = 10.0f;
            }
        } else if (mObjAcch.ChkGroundHit()) {
            if (mpMorfSO->isStop()) {
                initBackWalk();
            }
        }
        break;

    case 2:
        diff = (current.pos - field_0x678).absXZ();
        cLib_chaseAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &field_0x678),
                         diff < 500.0f ? (s16)0x800 : (s16)0x200);
        cLib_chaseF(&speedF, l_HIO.escape_speed, 1.0f);

        if (diff < 300.0f) {
            speedF = 0.0f;
            if (!cLib_addCalcPosXZ(&current.pos, field_0x678, 1.0f, l_HIO.escape_speed, 5.0f)) {
                mMode = 3;
            }
            if (mObjAcch.ChkWallHit()) {
                field_0x6cc++;
                if (field_0x6cc >= 30) {
                    mMode = 3;
                }
            } else {
                field_0x6cc = 0;
            }
        }
        break;

    case 3:
        if (cLib_chaseAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &home.pos), 0x400)) {
            setBck(5, 0, 5.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_WALK, -1);
            mSound.startCreatureSound(Z2SE_EN_HZ_WALK, 0, -1);
            mMode = 4;
        }
        break;

    case 4:
        if (mpMorfSO->checkFrame(4.0f)) {
            mSound.startCreatureSound(Z2SE_EN_HZ_WALK, 0, -1);
        } else if (mpMorfSO->checkFrame(10.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_BACK, -1);
        } else if (mpMorfSO->checkFrame(14.0f)) {
            speed.y = 0.0f;
            speedF = 0.0f;
            mMode = 5;
            field_0x6e4 = 0;
            setTgSetBit(0);
            mSpheres[0].OffTgSetBit();
            mSound.startCreatureSound(Z2SE_EN_HZ_SWING, 0, -1);
        }
        break;

    case 5:
        mDisableShadow = true;
        if (mpMorfSO->checkFrame(28.0f)) {
            mSound.startCreatureSound(Z2SE_EN_HZ_BACK, 0, -1);
        }
        if (current.pos.y <= home.pos.y) {
            current.pos.y = home.pos.y;
            speed.y = 0.0f;
        }
        cLib_addCalcPosXZ(&current.pos, home.pos, 1.0f, 20.0f, 5.0f);
        if (home.pos == current.pos) {
            if (mpMorfSO->isStop()) {
                setCloseSmokeEffect();
                shape_angle.y = (s16)(shape_angle.y + 0x2000) & 0xC000;
                gravity = 0.0f;
                setActionMode(ACTION_HIDE);
            }
        }
        break;
    }

    if (checkBck(0x11)) {
        if (mpMorfSO->checkFrame(0.0f)) {
            mSound.startCreatureSound(Z2SE_EN_HZ_WALK, 0, -1);
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_WALK, -1);
        } else if (mpMorfSO->checkFrame(15.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_WALK, -1);
        }
    }
    current.angle.y = shape_angle.y;
}

void daE_HZ_c::setWindEnd() {
    speed.y = 20.0f;
    field_0x6b2 = 0x1000;

#if TARGET_PC
    const dusk::coop::EnemyTargetResult target =
        dusk::coop::getEnemyTarget(this, dusk::coop::EnemyTargetScope::Combat);
    const f32 targetDistanceY =
        target.found && target.localActor != NULL
            ? target.localActor->current.pos.y - current.pos.y
            : fopAcM_searchPlayerDistanceY(this);
    if (std::abs(targetDistanceY) >= 300.0f)
#else
    if (std::abs(fopAcM_searchPlayerDistanceY(this)) >= 300.0f)
#endif
    {
        mWaitTimer = 5;
    } else {
        mWaitTimer = 10;
    }
    mMode = 4;
    field_0x6e4 = 1;
    gravity = -5.0f;
}

void daE_HZ_c::executeWind() {
    cXyz position;
    cXyz position2;
    dBgS_GndChk gndChk;
    dBgS_LinChk linChk;
    BOOL bVar = false;
    f32 deltaY;
    f32 frame = mpMorfSO->getFrame();
    f32 playerDist;
    f32 groundCross;
#if TARGET_PC
    mpBoomerangActor = coOpHzBoomerangActor(this);
#else
    mpBoomerangActor = daPy_py_c::getThrowBoomerangActor();
#endif

    switch (mMode) {
    case 0:
        speed.y = speedF = gravity = 0.0f;
        mSpheres[0].OffTgSetBit();
        setBck(0x13, 0, 0.0f, 1.0f);
        mSound.startCreatureSound(Z2SE_EN_HZ_WIND, 0, -1);
        mSound.startCreatureVoice(Z2SE_EN_HZ_V_WIND_NAKU, -1);
        speedF = speed.y = 0.0f;
        mMode = 1;
        field_0x6e4 = 0;

#if TARGET_PC
        playerDist = coOpHzDistanceToTarget(this, this);
#else
        playerDist = fopAcM_searchPlayerDistance(this);
#endif
        if (playerDist >= 1000.0f) {
            playerDist = 1000.0f;
        }
        if (playerDist < 100.0f) {
            playerDist = 100.0f;
        }

        field_0x6cc = (s16)((playerDist * 15.0f) / 1000.0f);
        /* fallthrough */
    case 1:
        deltaY = 6.0f - frame / 3.0f;
        if (deltaY < 0.0f) {
            deltaY = 0.0f;
        }

        current.pos.y += deltaY;
        ANGLE_SUB(shape_angle.y, 0x7D0);

        if (mpMorfSO->checkFrame(field_0x6cc) || mpBoomerangActor == NULL ||
            mpBoomerangActor->getReturnFlg())
        {
            bVar = true;
        }

        if (bVar) {
            mpMorfSO->setFrame(15.0f);
            mMode = 2;
            field_0x6a8 = 60.0f;
        }
        break;

    case 2:
        current.pos.y += field_0x6a8;
        if (current.pos.y - home.pos.y >= 300.0f) {
            if (frame < 23.0f) {
                mpMorfSO->setFrame(23.0f);
            }

            if (mpBoomerangActor == NULL) {
                setWindEnd();
            } else {
                position = mpBoomerangActor->current.pos;

                if (current.pos.y - home.pos.y >= 400.0f) {
                    if (cLib_chaseF(&field_0x6a8, 0.0f, 10.0f)) {
                        bVar = true;
                    }

                    if (mpBoomerangActor->getReturnFlg() &&
#if TARGET_PC
                        coOpHzDistanceToTarget(this, mpBoomerangActor) < 500.0f)
#else
                        fopAcM_searchPlayerDistance(mpBoomerangActor) < 500.0f)
#endif
                    {
                        bVar = true;
                    }
                }

                if (bVar) {
                    mMode = 3;
                    field_0x6e4 = 1;
                    speed.y = 0.0f;
                    position2 = position - current.pos;
                    field_0x6b2 = cLib_targetAngleY(&current.pos, &position);
                    field_0x678.x = position2.absXZ();
                    field_0x678.y = position2.y;
                    field_0x678.z = 8192.0f;
                    field_0x6b4 = 0x800;
                }
            }
        }
        break;

    case 3:
        if (mpBoomerangActor == NULL) {
            setWindEnd();
            return;
        }

        position = mpBoomerangActor->current.pos;
#if TARGET_PC
        if (coOpHzDistanceToTarget(this, this) < 500.0f &&
            mpBoomerangActor->getReturnFlg()) {
#else
        if (mPlayerDist < 500.0f && mpBoomerangActor->getReturnFlg()) {
#endif
            setWindEnd();
            return;
        }

        ANGLE_ADD(field_0x6b2, 0x800);
        linChk.Set(&current.pos, &position, NULL);
        if (!dComIfG_Bgsp().LineCross(&linChk)) {
            cLib_chaseF(&current.pos.x, position.x + field_0x678.x * cM_ssin(field_0x6b2), 50.0f);
            cLib_chaseF(&current.pos.z, position.z + field_0x678.x * cM_scos(field_0x6b2), 50.0f);
        } else {
            position.y += 10.0f;
        }

        cLib_chaseF(&field_0x678.x, 0.0f, 20.0f);
        gndChk.SetPos(&position);

        groundCross = dComIfG_Bgsp().GroundCross(&gndChk);
        if (frame < 38.0f) {
            if (position.y - groundCross < 500.0f) {
                position.y = groundCross + 500.0f;
            }
        } else if (position.y - groundCross < l_HIO.y_position_wrap) {
            position.y = groundCross + l_HIO.y_position_wrap;
        }

        cLib_chaseF(&current.pos.y, position.y + field_0x678.y, 20.0f);
        cLib_chaseF(&field_0x678.y, 0.0f, 5.0f);
        cLib_chaseAngleS(&shape_angle.z, field_0x678.z * cM_scos(field_0x6b4), 0x100);

        if (mpMorfSO->isStop()) {
            cLib_chaseAngleS(&shape_angle.x, -0x8000, 0x400);
        }

        ANGLE_SUB(shape_angle.y, l_HIO.reeling_rotation_speed);
        break;

    case 4:
        cLib_chaseAngleS(&shape_angle.x, -0x8000, 0x800);
        cLib_chaseAngleS(&shape_angle.z, field_0x678.z * cM_scos(field_0x6b4), 0x100);
        cLib_chaseF(&field_0x678.z, 0.0f, 512.0f);

        if (mWaitTimer != 0) {
            speed.y = 20.0f;
        } else if (mObjAcch.ChkGroundHit()) {
            setTgSetBit(1);
            mPiyoriTimer = l_HIO.piyori_time;
            setBck(0xE, 0, 0.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_LANDING, -1);
            mSound.startCreatureSound(Z2SE_EN_HZ_LANDING, 0, -1);
            mSound.startCreatureSound(Z2SE_CM_BODYFALL_M, 0, -1);
            shape_angle.x = 0;
            speed.y = 15.0f;
            mMode = 5;
        }

        shape_angle.y -= field_0x6b2;
        break;

    case 5:
        if (field_0x6b2 > 0x400) {
            mSound.startCreatureSoundLevel(Z2SE_EN_HZ_SLIDE, 0, -1);
        }
        cLib_chaseAngleS(&shape_angle.z, field_0x678.z * cM_scos(field_0x6b4), 0x100);
        cLib_chaseF(&field_0x678.z, 0.0f, 512.0f);
        if (mpMorfSO->checkFrame(13.0f)) {
            mSound.startCreatureSound(Z2SE_CM_BODYFALL_S, 0, -1);
        }
        if (mObjAcch.ChkGroundHit()) {
            cLib_chaseAngleS(&field_0x6b2, 0, 0x100);
            if (field_0x6b2 == 0) {
                if (mpMorfSO->isStop()) {
                    setActionMode(ACTION_CHANCE);
                    return;
                }
            }
        }

        shape_angle.y -= field_0x6b2;
        break;
    }

    if (checkBck(dRes_ID_E_HZ_BCK_HZ_WIND_e) && mpMorfSO->checkFrame(25.0f)) {
        mSound.startCreatureVoice(Z2SE_EN_HZ_V_WIND, -1);
    }
}

void daE_HZ_c::executeChance() {
    switch (mMode) {
    case 0:
        gravity = -5.0f;
        field_0x6e4 = 1;
        setBck(dRes_ID_E_HZ_BCK_HZ_FAINT_e, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        setTgSetBit(1);
        mSpheres[0].OnTgSetBit();
        mSpheres[0].SetTgType(0x10000);
        mMode = 1;
        shape_angle.x = shape_angle.z = 0;
        speedF = 0.0f;
        /* fallthrough */

    case 1:
        if (mpMorfSO->checkFrame(0.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_FAINT, -1);
        }
        if (mPiyoriTimer == 0) {
            setTgSetBit(0);
            setActionMode(ACTION_AWAY);
        }
        break;
    }
}

void daE_HZ_c::initRollDamage() {
    current.angle.y = mAtInfo.mHitDirection.y + 0x8000;
    shape_angle.x = shape_angle.z = 0;

    if (mAction == ACTION_DEATH) {
        if (field_0x6cc == 0) {
            speedF = 20.0f;
            field_0x6b6 = 0x1000;
            return;
        }

        if (field_0x6cc == 1) {
            speedF = 35.0f;
            field_0x6b6 = 0x1400;
            return;
        }

        if (field_0x6cc == 2) {
            speedF = 20.0f;
            field_0x6b6 = 0;
            return;
        }

        speedF = 30.0f;
        field_0x6b6 = 0x800;
    } else {
        if (field_0x6cc == 0) {
            speedF = 10.0f;
            field_0x6b6 = 0;
            return;
        }

        if (field_0x6cc == 1) {
            speedF = 30.0f;
            field_0x6b6 = 0x1000;
            return;
        }

        speedF = 20.0f;
        field_0x6b6 = 0;
    }
}

bool daE_HZ_c::doRollDamage() {
    shape_angle.y += field_0x6b6;

    if (mObjAcch.ChkGroundHit()) {
        if (speedF >= 3.0f) {
            fopAcM_effSmokeSet2(&mSmokeKey, &mSmokeKey2, &current.pos, NULL, 1.5f, &tevStr);
        }

        cLib_chaseF(&speedF, 0.0f, 1.0f);
        cLib_chaseAngleS(&field_0x6b6, 0, 0x80);
        if (!speedF && field_0x6b6 == 0 && mpMorfSO->isStop()) {
            return true;
        }
    }

    return false;
}

void daE_HZ_c::executeDamage() {
    switch (mMode) {
    case 0:
        gravity = -5.0f;
        mSpheres[0].OffTgSetBit();
        field_0x6e4 = 1;
        initRollDamage();
        if (mPrevAction == 3) {
            mMode = 2;
            setBck(dRes_ID_E_HZ_BCK_HZ_WALK_D_R_e, J3DFrameCtrl::EMode_NONE, 5.0f, 1.0f);
        } else {
            mMode = 1;
            setBck(dRes_ID_E_HZ_BCK_HZ_DAMAGE_e, J3DFrameCtrl::EMode_NONE, 5.0f, 1.0f);
        }

        mSound.startCreatureVoice(Z2SE_EN_HZ_V_DAMAGE, -1);
        mpMorfSO->setFrame(0.0f);
        break;

    case 1:
        if (speedF > 2.0f) {
            mSound.startCreatureSoundLevel(Z2SE_EN_HZ_SLIDE, 0, -1);
        }

        if (mpMorfSO->checkFrame(28.0f)) {
            mSound.startCreatureSound(Z2SE_CM_BODYFALL_S, 0, -1);
        }

        if (doRollDamage()) {
            setActionMode(ACTION_CHANCE);
            break;
        }

        if (mPiyoriTimer == 0 && !speedF) {
            speedF = 0.0f;
            setTgSetBit(0);
            setActionMode(ACTION_AWAY);
        }
        break;

    case 2:
        if (doRollDamage()) {
            setActionMode(ACTION_AWAY);
            mMode = 1;
        }
        break;
    }
}

void daE_HZ_c::executeDeath() {
    mDamageDeathTimer = 10;
    field_0x566 = 0;

    switch (mMode) {
    case 0:
        if (mpBgW != NULL) {
            dComIfG_Bgsp().Release(mpBgW);
            mpBgW = NULL;
        }
        gravity = -5.0f;
        mSpheres[0].OffTgSetBit();
        setTgSetBit(0);
        field_0x6e4 = 1;
        mMode = 1;

        if (field_0x6cc >= 2) {
            setBck(8, 0, 5.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_DEATH_STAND, -1);
        } else {
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_DEATH, -1);
            if (mPrevAction == 7) {
                setBck(7, 0, 5.0f, 1.0f);
            } else if (mPrevAction == 3) {
                setBck(7, 0, 10.0f, 1.0f);
            } else {
                setBck(7, 0, 0.0f, 1.0f);
            }
        }
        initRollDamage();
        /* fallthrough */

    case 1:
        if (checkBck(7)) {
            if (mpMorfSO->checkFrame(41.5f)) {
                mSound.startCreatureSound(Z2SE_CM_BODYFALL_M, 0, -1);
            }
            if (speedF > 2.0f) {
                mSound.startCreatureSoundLevel(Z2SE_EN_HZ_SLIDE, 0, -1);
            }
            if (mpMorfSO->checkFrame(35.0f)) {
                mReadyChangeColor = 1;
            }
        } else {
            if (mpMorfSO->isStop()) {
                mReadyChangeColor = 1;
            }
        }
        if (doRollDamage()) {
            mMode = 2;
        }
        break;

    case 2:
        if (mpMorfSO->isStop()) {
            fopAcM_createDisappear(this, &current.pos, 10, 0, 5);
            setActionMode(0xB);
        }
        break;
    }

    if (mReadyChangeColor != 0) {
        cLib_addCalc2(&mColor, -20.0f, 1.0f, 0.4f);
    }
}

void daE_HZ_c::setWindChanceEnd() {
    mWaitTimer = 30;
    mMode = 2;
    field_0x6b6 = 0x1800;
}

void daE_HZ_c::executeWindChance() {
    cXyz end;
    cXyz start;
    dBgS_LinChk linChk;

#if TARGET_PC
    mpBoomerangActor = coOpHzBoomerangActor(this);
#else
    mpBoomerangActor = daPy_py_c::getThrowBoomerangActor();
#endif

    switch (mMode) {
    case 0:
        setTgSetBit(0);
        mSpheres[0].OffTgSetBit();
        setBck(0xB, 2, 3.0f, 1.0f);
        mSound.startCreatureVoice(Z2SE_EN_HZ_V_FAINT, -1);
        mMode = 1;
        field_0x6b6 = 0;
        field_0x6b2 = l_HIO.reeling_rotation_speed;
        field_0x6b4 = 0;
        /* fallthrough */
    case 1:
        if (mpBoomerangActor == NULL) {
            setWindChanceEnd();
            return;
        }

        end = mpBoomerangActor->current.pos;
        ANGLE_ADD(field_0x6b6, 0x800);
        start = current.pos;
        start.y += 50.0f;
        linChk.Set(&start, &end, NULL);

        if (dComIfG_Bgsp().LineCross(&linChk)) {
            setWindChanceEnd();
            return;
        }

        cLib_chaseF(&current.pos.x, end.x + field_0x678.x * cM_ssin(field_0x6b6), 30.0f);
        cLib_chaseF(&current.pos.z, end.z + field_0x678.x * cM_scos(field_0x6b6), 30.0f);
        cLib_chaseF(&field_0x678.x, 0.0f, 20.0f);

        if (mObjAcch.ChkGroundHit()) {
            speed.y = 20.0f;
        }

        shape_angle.y -= field_0x6b2;
        ANGLE_ADD(field_0x6b4, 0x1000);
        shape_angle.x = (s16)(cM_scos(field_0x6b4) * 6144.0f);
        break;
        
    default:
        break;

    case 2:
        cLib_chaseAngleS(&field_0x6b2, 0, 0x100);
        cLib_chaseAngleS(&field_0x6b6, 0, 0x200);
        shape_angle.y -= field_0x6b2;
        ANGLE_ADD(field_0x6b4, 0x1000);
        shape_angle.x = (s16)(field_0x6b6 * cM_scos(field_0x6b4));
        
        if (mWaitTimer == 0) {
            if (mPiyoriTimer != 0) {
                S16_ADD(mPiyoriTimer, 20);
            }

            setActionMode(ACTION_CHANCE);
        }
    }
}

void daE_HZ_c::executeWindWalk() {
    switch (mMode) {
    case 0:
        setTgSetBit(0);
        mSpheres[0].OffTgSetBit();
        setBck(0xC, 0, 3.0f, 1.0f);
        mSound.startCreatureVoice(Z2SE_EN_HZ_V_GALE, -1);
        mSound.startCreatureSound(Z2SE_EN_HZ_WALK, 0, -1);
        mMode = 1;
        speedF = 0.0f;
        /* fallthrough */

    case 1:
        if (mpMorfSO->checkFrame(14.0f) || mpMorfSO->checkFrame(24.0f)) {
            mSound.startCreatureSound(Z2SE_EN_HZ_WALK, 0, -1);
        }
        if (mpMorfSO->isStop()) {
            setActionMode(ACTION_AWAY);
            initBackWalk();
        }
        break;
    }
}

void daE_HZ_c::setWaterEffect() {
    cXyz position(current.pos.x, mGroundCross, current.pos.z);
    static cXyz sc(2.0f, 2.0f, 2.0f);
    static u16 w_eff_id[4] = {
        ID_ZI_J_DOWNWTRA_A,
        ID_ZI_J_DOWNWTRA_B,
        ID_ZI_J_DOWNWTRA_C,
        ID_ZI_J_DOWNWTRA_D,
    };

    for (int i = 0; i < 4; i++) {
        mWaterEffects[i] = dComIfGp_particle_set(mWaterEffects[i], w_eff_id[i], &position, &tevStr,
                                                 0, &sc, 0xFF, NULL, -1, NULL, NULL, NULL);
    }
}

void daE_HZ_c::executeWaterDeath() {
    field_0x566 = 0;

    switch (mMode) {
    case 0:
        speedF = gravity = 0.0f;
        mMode = 1;
        setWaterEffect();
        mSound.startCreatureSound(Z2SE_CM_BODYFALL_WATER_L, 0, -1);
        if (checkBck(0x13)) {
            setBck(0xA, 0, 0.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_DROWNED_B, -1);
        } else if (checkBck(5) || checkBck(0xC) || checkBck(0xF) || checkBck(0x11) ||
                   checkBck(0x12))
        {
            setBck(9, 0, 3.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_DROWNED_A, -1);
        } else {
            setBck(0xA, 0, 3.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_HZ_V_DROWNED_B, -1);
        }
        /* fallthrough */
    case 1:
        cLib_chaseF(&speed.y, 5.0f, 10.0f);
        cLib_chaseAngleS(&shape_angle.x, 0, 0x800);
        cLib_chaseAngleS(&shape_angle.z, 0, 0x800);
        if (mpMorfSO->checkFrame(10.0f) || mpMorfSO->checkFrame(20.0f) ||
            mpMorfSO->checkFrame(36.0f) || mpMorfSO->checkFrame(57.0f))
        {
            setWaterEffect();
            mSound.startCreatureSound(Z2SE_EN_HZ_DROWNSPLASH, 0, -1);
        }
        if (speed.y > 0.0f) {
            if (current.pos.y + 100.0f > mGroundCross) {
                current.pos.y = mGroundCross - 100.0f;
                mMode = 2;
                speed.y = 0.0f;
            }
        }
        break;

    case 2:
        if (mpMorfSO->checkFrame(10.0f) || mpMorfSO->checkFrame(20.0f) ||
            mpMorfSO->checkFrame(36.0f) || mpMorfSO->checkFrame(57.0f))
        {
            setWaterEffect();
            mSound.startCreatureSound(Z2SE_EN_HZ_DROWNSPLASH, 0, -1);
        }
        if (mpMorfSO->isStop()) {
            cXyz position;

            mDoMtx_stack_c::copy(mpMorfSO->getModel()->getAnmMtx(0x12));
            mDoMtx_stack_c::multVecZero(&position);
            fopAcM_createDisappear(this, &position, 10, 0, 5);
            setActionMode(0xB);
        }
        break;
    }

    if (current.pos.y + 150.0f < mGroundCross && mGroundCross < current.pos.y + 50.0f) {
        cXyz position(current.pos.x, mGroundCross, current.pos.z);

        fopAcM_effHamonSet(&mEffect, &position, 1.0f, 1.0f);
    }
}

void daE_HZ_c::executeDeathWait() {
    field_0x566 = 0;

    switch (mMode) {
    case 0:
        dComIfGs_onSwitch(mParam, fopAcM_GetRoomNo(this));
        mSound.deleteObject();
        field_0x6e8 = 1;
        attention_info.flags = 0;
        attention_info.position = eyePos = current.pos = home.pos;
        fopAcM_SetGroup(this, 0);
        mMode = 3;
        break;

    case 1:
        mSound.deleteObject();
        field_0x6e8 = 1;
        attention_info.flags = 0;
        attention_info.position = eyePos = current.pos = home.pos;
        fopAcM_SetGroup(this, 0);
        mMode = 4;
        break;
    }
}

void daE_HZ_c::damage_check() {
    s16 targetAngleY;
    
    if (mAction != 1 || mMode < 4) {
        if (mSpheres[0].ChkTgHit() && mSpheres[0].GetTgHitObj()->ChkAtType(AT_TYPE_BOOMERANG)) {
#if TARGET_PC
            const dusk::coop::damage_owner::DamageOwnerResult boomerangOwner =
                dusk::coop::damage_owner::resolveDamageOwner(this, mSpheres[0].GetTgHitObj());
            dusk::coop::damage_owner::recordDamageOwnerHit(
                "e_hz.boomerang", this, boomerangOwner, NULL);
            // Co-op: wind/reel continuation follows the player whose boomerang caused this hit.
            coOpHzSelectExactPlayer(this, boomerangOwner.localPlayerActor,
                                    "e_hz.boomerang_owner");
            if (boomerangOwner.hitActor != NULL &&
                fopAcM_GetName(boomerangOwner.hitActor) == fpcNm_BOOMERANG_e)
            {
                mpBoomerangActor = static_cast<daBoomerang_c*>(boomerangOwner.hitActor);
            }
#endif
            mSpheres[0].ClrTgHit();
            if (mAction == ACTION_CHANCE) {
                setActionMode(ACTION_WIND_CHANCE);
            } else if (mAction == ACTION_AWAY) {
                setActionMode(ACTION_WIND_WALK);
            } else {
                setActionMode(ACTION_WIND);
            }
            setTgSetBit(0);
            mDamageDeathTimer = 10;
        } else if (mDamageDeathTimer == 0) {
            mStts.Move();
#if !TARGET_PC
            cXyz player_pos(daPy_getPlayerActorClass()->current.pos);
#endif

            s32 bVar = 0;

            for (int i = 1; i <= 2; i++) {
                if (mSpheres[i].ChkTgHit()) {
                    mAtInfo.mpCollider = mSpheres[i].GetTgHitObj();

                    if (mAction == ACTION_ATTACK) {
                        mAtInfo.field_0x18 = 0x2a;
                        mAtInfo.mpCollider->SetAtAtp(0);
                    } else {
                        mAtInfo.field_0x18 = 0;
                    }

                    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SLINGSHOT)) {
                        mSound.startCreatureSound(Z2SE_HIT_PACHINKO, 0x2D, -1);
                        return;
                    }

                    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
                        bVar = true;
                        health = 0;
                    }

#if TARGET_PC
                    const dusk::coop::damage_owner::DamageOwnerResult damageOwner =
                        dusk::coop::damage_owner::resolveDamageOwner(this, mAtInfo.mpCollider);
                    daPy_py_c* damagePlayer =
                        dusk::coop::damage_owner::resolveDamageOwnerPlayer(damageOwner);
                    cXyz player_pos =
                        damagePlayer != NULL ? damagePlayer->current.pos
                                             : daPy_getPlayerActorClass()->current.pos;
#endif
                    cc_at_check(this, &mAtInfo);
                    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_UNK)) {
                        mDamageDeathTimer = 0x14;
                    } else if (
#if TARGET_PC
                        coOpHzRightTurnCut(damageOwner.cutType)
#else
                        cc_pl_cut_bit_get() == 0x80
#endif
                    ) {
                        mDamageDeathTimer = 0x14;
                    } else {
                        mDamageDeathTimer = 10;
                    }

                    if (mAtInfo.mAttackPower <= 1) {
                        mDamageDeathTimer = KREG_S(8) + 10;
                    }

                    mSpheres[1].ClrTgHit();
                    mSpheres[2].ClrTgHit();
                    mSpheres[3].ClrTgHit();

                    if (mAction == ACTION_ATTACK) {
#if TARGET_PC
                        dusk::coop::damage_owner::recordDamageOwnerHit(
                            "e_hz.attack_hit", this, damageOwner, &mAtInfo);
#endif
                        setActionMode(ACTION_HIDE);
                        return;
                    }

                    if (
#if TARGET_PC
                        damageOwner.cutCount >= 4 ||
#else
                        daPy_getPlayerActorClass()->getCutCount() >= 4 ||
#endif
                        ((dCcD_GObjInf*)mAtInfo.mpCollider)->GetAtSpl() == 1)
                    {
                        bVar = true;
                    }

                    if (health < 1) {
                        field_0x6cc = 0;
                        if (bVar) {
                            field_0x6cc = 1;
                        }

                        if (mAction == ACTION_AWAY) {
                            field_0x6cc += 2;
                        }

                        setActionMode(ACTION_DEATH);
                    } else {
                        if (mAction == ACTION_AWAY) {
                            if (bVar) {
                                field_0x6cc = 1;
                                setActionMode(ACTION_DAMAGE);
                                return;
                            }

                            targetAngleY = cLib_targetAngleY(&mSpheres[1].GetCoCP(), &player_pos);
                            targetAngleY = (s16)(targetAngleY - shape_angle.y);
                            if (i == 1) {
                                if (abs(targetAngleY) >= 0x4000) {
                                    if (bVar) {
                                        mBackbone1YZRot.z = 0x5000;
                                        mBackbone2YZRot.z = -0x4000;
                                        mBackbone3YZRot.z = 0;
                                    } else {
                                        mBackbone1YZRot.z = 0x3000;
                                    }

                                } else if (bVar) {
                                    mBackbone1YZRot.z = -0x3000;
                                    mBackbone2YZRot.z = -0x2000;
                                    mBackbone3YZRot.z = -0x1000;
                                } else {
                                    mBackbone1YZRot.z = -0x2000;
                                    mBackbone2YZRot.z = -0x1000;
                                    mBackbone3YZRot.z = -0x800;
                                }
                                speedF = 0.0f;
                            } else if (bVar) {
                                mBackbone1YZRot.z = 0x5000;
                                mBackbone2YZRot.z = -0x4000;
                                mBackbone3YZRot.z = 0;
                            } else {
                                mBackbone1YZRot.z = 0x3000;
                            }

                            if (targetAngleY < -0x2800) {
                                targetAngleY = -0x2800;
                            }

                            if (targetAngleY > 0x2800) {
                                targetAngleY = 0x2800;
                            }

                            mBackbone1YZRot.y = targetAngleY;
                            mSound.startCreatureVoice(Z2SE_EN_HZ_V_DAMAGE, -1);
                            return;
                        }

                        if (bVar) {
                            field_0x6cc = 1;
                        } else {
                            field_0x6cc = 0;
                            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
                                field_0x6cc = 2;
                            }
                        }

                        setActionMode(ACTION_DAMAGE);
                    }
#if TARGET_PC
                    dusk::coop::damage_owner::recordDamageOwnerHit(
                        "e_hz.damage", this, damageOwner, &mAtInfo, mAction);
#endif
                    return;
                }
            }
        }
    }
}

bool daE_HZ_c::checkWaterSurface() {
    dBgS_ObjGndChk_Spl spline;

    Vec modified_pos;
    modified_pos = current.pos;
    modified_pos.y += 500.0f;
    spline.SetPos(&modified_pos);
    mGroundCross = dComIfG_Bgsp().GroundCross(&spline);

    if (speed.y < 0.0f && mGroundCross > current.pos.y + 100.0f) {
        return true;
    }
    return false;
}

void daE_HZ_c::action() {
    cXyz unused;
    cXyz unused2;

#if TARGET_PC
    CoOpHzTargetState target;
    const bool committed =
        mAction == ACTION_ATTACK || mAction == ACTION_WIND || mAction == ACTION_WIND_CHANCE ||
        mAction == ACTION_WIND_WALK ||
        coOpHzThrowOwner(this, "e_hz.action_throw").active;
    const dusk::coop::EnemyTargetMode mode =
        mAction == ACTION_WAIT || mAction == ACTION_HIDE
            ? dusk::coop::EnemyTargetMode::ImmediateAcquire
            : dusk::coop::EnemyTargetMode::StickyCombat;
    if (coOpSelectHzTargetState(this, "e_hz.action", committed, mode, &target)) {
        mPlayerDist = target.distance;
        mPlayerAngleY = target.angleY;
    } else {
        mPlayerDist = fopAcM_searchPlayerDistance(this);
        mPlayerAngleY = fopAcM_searchPlayerAngleY(this);
    }
#else
    mPlayerDist = fopAcM_searchPlayerDistance(this);
    mPlayerAngleY = fopAcM_searchPlayerAngleY(this);
#endif
    damage_check();

    if (mAction != 10 && checkWaterSurface()) {
        setActionMode(ACTION_WATER_DEATH);
    }

    attention_info.flags = fopAc_AttnFlag_BATTLE_e;
    field_0x566 = 1;

    switch (mAction) {
    case ACTION_WAIT:
        executeWait();
        break;

    case ACTION_HIDE:
        attention_info.flags = 0;
        executeHide();
        break;

    case ACTION_ATTACK:
        executeAttack();
        break;

    case ACTION_AWAY:
        executeAway();
        break;

    case ACTION_WIND:
        executeWind();
        break;

    case ACTION_DAMAGE:
        executeDamage();
        break;

    case ACTION_DEATH:
        attention_info.flags = 0;
        executeDeath();
        break;

    case ACTION_CHANCE:
        executeChance();
        break;

    case ACTION_WIND_CHANCE:
        executeWindChance();
        break;

    case ACTION_WIND_WALK:
        executeWindWalk();
        break;

    case ACTION_WATER_DEATH:
        attention_info.flags = 0;
        executeWaterDeath();
        break;

    case ACTION_DEATH_WAIT:
        attention_info.flags = 0;
        executeDeathWait();
        break;
    }

    if (mAction == ACTION_WAIT || mAction == ACTION_HIDE) {
        mDisableShadow = true;
    }

    if (field_0x566 != 0) {
        cXyz effPos;

        mDoMtx_stack_c::copy(mpMorfSO->getModel()->getAnmMtx(0x12));
        mDoMtx_stack_c::multVecZero(&effPos);

        cXyz effSize(2.0f, 2.0f, 2.0f);

        setMidnaBindEffect(this, &mSound, &effPos, &effSize);
    }

    checkFall();
    mSound.setLinkSearch(false);
    fopAcM_posMoveF(this, NULL);

    if (field_0x6e4 != 0) {
        mObjAcch.CrrPos(dComIfG_Bgsp());
    } else {
        mObjAcch.ClrGroundHit();
    }

    mpMorfSO->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

void daE_HZ_c::mtx_set() {
    mDoMtx_stack_c::transS(home.pos.x, home.pos.y - 13.0f, home.pos.z);
    mpModel2->setBaseTRMtx(mDoMtx_stack_c::get());

    if (field_0x6e8 != 0) {
        return;
    }

    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mDoMtx_stack_c::scaleM(l_HIO.basic_size, l_HIO.basic_size, l_HIO.basic_size);
    J3DModel* morfModel = mpMorfSO->getModel();
    morfModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorfSO->modelCalc();
    MtxP anmMtx = morfModel->getAnmMtx(7);
    mpModel->setBaseTRMtx(anmMtx);
    mSmokeEffectPosition.set(anmMtx[0][3], anmMtx[1][3], anmMtx[2][3]);
    if (mSetModelAnmMtx) {
        mSetModelAnmMtx = false;
        mDoMtx_stack_c::copy(anmMtx);
        mDoMtx_stack_c::ZrotM(-0x4000);
        mDoMtx_stack_c::transM(0.0f, 14.0f, 0.0f);
        cMtx_copy(mDoMtx_stack_c::get(), mMtx);
    } else {
        mDoMtx_stack_c::transS(home.pos);
        cMtx_copy(mDoMtx_stack_c::get(), mMtx);
    }

    if (mpBgW != NULL) {
        mpBgW->Move();
    }
}

void daE_HZ_c::cc_set() {
    if (field_0x6e8 != 0) {
        return;
    }

    cXyz pos;
    J3DModel* morfModel = mpMorfSO->getModel();

    attention_info.position = eyePos = mSmokeEffectPosition;
    attention_info.position.y += 30.0f;

    MTXCopy(morfModel->getAnmMtx(7), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&pos);
    mSpheres[0].SetC(pos);
    mSpheres[0].SetR(80.0f);
    dComIfG_Ccsp()->Set(&mSpheres[0]);

    MTXCopy(morfModel->getAnmMtx(1), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&pos);
    mSpheres[1].SetC(pos);
    mSpheres[1].SetR(80.0f);
    dComIfG_Ccsp()->Set(&mSpheres[1]);

    MTXCopy(morfModel->getAnmMtx(0x12), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&pos);
    mSpheres[2].SetC(pos);
    mSpheres[2].SetR(80.0f);
    dComIfG_Ccsp()->Set(&mSpheres[2]);
}

int daE_HZ_c::execute() {
    if (field_0x6e8 != 0) {
        mtx_set();
        attention_info.position = eyePos = current.pos;

        return 1;
    }

    if (mWaitTimer != 0) {
        mWaitTimer--;
    }
    if (mDamageDeathTimer != 0) {
        mDamageDeathTimer--;
    }
    if (mPiyoriTimer != 0) {
        mPiyoriTimer--;
    }
    if (mAttackStartTimer != 0) {
        mAttackStartTimer--;
    }
    if (mRetentionBeforeStretchTimer != 0) {
        mRetentionBeforeStretchTimer--;
    }
    if (mAttackCooldownTimer != 0) {
        mAttackCooldownTimer--;
    }

    if (mCameraOffTimer != 0) {
        mCameraOffTimer--;
        if (mCameraOffTimer == 0) {
#if TARGET_PC
            dCamera_c* throwCamera = coOpHzThrowCamera(this, "e_hz.camera_off");
            if (throwCamera != NULL && throwCamera->GetForceLockOnActor() == this) {
                throwCamera->ForceLockOff(this);
            }
#else
            if (dCam_getBody()->GetForceLockOnActor() == this) {
                dCam_getBody()->ForceLockOff(this);
            }
#endif
        }
    }

#if TARGET_PC
    dCamera_c* throwCamera = coOpHzThrowCamera(this, "e_hz.camera_height");
    daPy_py_c* throwPlayer = coOpHzThrowPlayer(this, "e_hz.camera_height");
    if (throwCamera != NULL && throwCamera->GetForceLockOnActor() == this &&
        throwPlayer->current.pos.y < current.pos.y - 100.0f)
#else
    if (dCam_getBody()->GetForceLockOnActor() == this &&
        daPy_getPlayerActorClass()->current.pos.y < current.pos.y - 100.0f)
#endif
    {
#if TARGET_PC
        throwCamera->ForceLockOff(this);
#else
        dCam_getBody()->ForceLockOff(this);
#endif
    }

    if (mCameraOnTimer != 0) {
        mCameraOnTimer--;
        if (mCameraOnTimer == 0) {
#if TARGET_PC
            dCamera_c* throwCamera = coOpHzThrowCamera(this, "e_hz.camera_on");
            if (throwCamera != NULL && throwCamera->GetForceLockOnActor() != this) {
                throwCamera->ForceLockOn(this);
            }
#else
            if (dCam_getBody()->GetForceLockOnActor() != this) {
                dCam_getBody()->ForceLockOn(this);
            }
#endif
        }
    }

    cLib_chaseAngleS(&mBackbone1YZRot.z, 0, 0x200);
    cLib_chaseAngleS(&mBackbone1YZRot.y, 0, 0x200);
    cLib_chaseAngleS(&mBackbone2YZRot.z, 0, 0x200);
    cLib_chaseAngleS(&mBackbone2YZRot.y, 0, 0x200);
    cLib_chaseAngleS(&mBackbone3YZRot.z, 0, 0x200);
    cLib_chaseAngleS(&mBackbone3YZRot.y, 0, 0x200);
    action();
    mtx_set();
    cc_set();

#if TARGET_PC
    const dusk::coop::retained_interaction_owner::RetainedInteractionState throwOwner =
        coOpHzThrowOwner(this, "e_hz.throw_release");
    dCamera_c* throwCameraEnd = coOpHzThrowCamera(this, "e_hz.throw_release");
    // Co-op: event acceptance may span frames before throw damage starts; do not release early.
    const bool waitingForThrow =
        mAction == ACTION_HIDE && (mMode == 4 || mMode == 5);
    if (throwOwner.active && !waitingForThrow && !mReadyNewAction &&
        mRetentionBeforeStretchTimer == 0 &&
        mCameraOnTimer == 0 && mCameraOffTimer == 0 &&
        (throwCameraEnd == NULL || throwCameraEnd->GetForceLockOnActor() != this) &&
        (throwOwner.localPlayer == NULL || !throwOwner.localPlayer->checkThrowDamage()))
    {
        // Co-op: release only after native throw damage and the owner camera's force lock end.
        coOpClearHzThrow(this, "e_hz.throw_release");
    }
#endif

    return 1;
}

static int daE_HZ_Execute(daE_HZ_c* i_this) {
    return i_this->execute();
}

static int daE_HZ_IsDelete(daE_HZ_c* i_this) {
    return 1;
}

int daE_HZ_c::_delete() {
#if TARGET_PC
    // Co-op: release combat, retained victim, and owner-camera force-lock state before reuse.
    coOpClearHzThrow(this, "e_hz.delete");
    dusk::coop::clearAllEnemyTargets(this);
    dusk::coop::retained_interaction_owner::clearAllRetainedInteractions(this);
#endif
    dComIfG_resDelete(&mPhaseReq, "E_HZ");
    dComIfG_resDelete(&mPhaseReq2, mpName);

    if (mHIOInit) {
        hio_set = false;
        mDoHIO_DELETE_CHILD(l_HIO.enemy_sample);
    }

    if (heap != NULL) {
        mSound.deleteObject();
    }

    if (mpBgW != NULL) {
        dComIfG_Bgsp().Release(mpBgW);
    }

    return 1;
}

static int daE_HZ_Delete(daE_HZ_c* i_this) {
    fopAcM_RegisterDeleteID(i_this, "E_HZ");
    return i_this->_delete();
}

int daE_HZ_c::ctrlJoint(J3DJoint* i_joint, J3DModel* i_model) {
    J3DJoint* joint = i_joint;
    int joint_no = joint->getJntNo();

    mDoMtx_stack_c::copy(i_model->getAnmMtx(joint_no));

    switch (joint_no) {
    case HZ_JNT_BACK_BONE1_e:
        mDoMtx_stack_c::ZrotM(mBackbone1YZRot.z);
        mDoMtx_stack_c::YrotM(mBackbone1YZRot.y);
        break;

    case HZ_JNT_BACK_BONE2_e:
        mDoMtx_stack_c::ZrotM(mBackbone2YZRot.z);
        mDoMtx_stack_c::YrotM(mBackbone2YZRot.y);
        break;

    case HZ_JNT_BACK_BONE3_e:
        mDoMtx_stack_c::ZrotM(mBackbone3YZRot.z);
        mDoMtx_stack_c::YrotM(mBackbone3YZRot.y);
        break;
    }

    i_model->setAnmMtx(joint_no, mDoMtx_stack_c::get());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    return 1;
}

int daE_HZ_c::JointCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        J3DModel* model = j3dSys.getModel();
        daE_HZ_c* a_this = (daE_HZ_c*)model->getUserArea();
        if (a_this != NULL) {
            a_this->ctrlJoint(i_joint, model);
        }
    }

    return 1;
}

int daE_HZ_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_HZ", dRes_INDEX_E_HZ_BMD_HZ_e);
    JUT_ASSERT(2479, modelData != NULL);

    mpMorfSO = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL,
                                    (J3DAnmTransform*)dComIfG_getObjectRes("E_HZ", dRes_INDEX_E_HZ_BCK_HZ_WAIT_e), 0, 1.0f,
                                    0, -1, &mSound, J3DMdlFlag_DifferedDLBuffer, 0x11000084);

    if (mpMorfSO == NULL || mpMorfSO->getModel() == NULL) {
        return 0;
    }

    // This happens to work with MWCC since the member will only ever be initialized a pointer to a
    // string in this TU's .data section, but comparing against a string literal is still UB.
#if AVOID_UB
    if (strcmp(mpName, "E_hzp") == 0) {
#else
    if (mpName == "E_hzp") {
#endif
        modelData = (J3DModelData*)dComIfG_getObjectRes(mpName, 3);
    } else {
        modelData = (J3DModelData*)dComIfG_getObjectRes(mpName, 3);
    }

    JUT_ASSERT(2505, modelData != NULL);

    mpModel = mDoExt_J3DModel__create(modelData, J3DMdlFlag_DifferedDLBuffer, 0x11000084);
    if (mpModel == NULL) {
        return 0;
    }

    mpBgW = JKR_NEW dBgW();
    if (mpBgW == NULL) {
        return 0;
    }

    if (mpBgW->Set((cBgD_t*)dComIfG_getObjectRes("E_HZ", dRes_INDEX_E_HZ_DZB_K_HZP00_e), 1, &mMtx) == 1) {
        return 0;
    }

    mpBgW->SetCrrFunc(dBgS_MoveBGProc_Typical);

    modelData = (J3DModelData*)dComIfG_getObjectRes("E_HZ", dRes_INDEX_E_HZ_BMD_HZ_HOLE_e);
    JUT_ASSERT(2532, modelData != NULL);

    mpModel2 = mDoExt_J3DModel__create(modelData, J3DMdlFlag_DifferedDLBuffer, 0x11000084);
    if (mpModel2 == NULL) {
        return 0;
    }

    J3DModel* morfModel = mpMorfSO->getModel();
    morfModel->setUserArea((uintptr_t)this);
    for (u16 i = 0; i < morfModel->getModelData()->getJointNum(); i++) {
        if (i != 0) {
            morfModel->getModelData()->getJointNodePointer(i)->setCallBack(JointCallBack);
        }
    }
    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    daE_HZ_c* actor = (daE_HZ_c*)i_this;
    return actor->CreateHeap();
}

void daE_HZ_c::setInitPos() {
    dBgS_GndChk gnd_chk;
    cXyz modified_home_pos(home.pos);

    modified_home_pos.y += 100.0f;
    gnd_chk.SetPos(&modified_home_pos);

    f32 ground_cross = dComIfG_Bgsp().GroundCross(&gnd_chk);
    if (ground_cross != -G_CM3D_F_INF) {
        home.pos.y = ground_cross;
        old.pos.y = ground_cross;
        current.pos.y = ground_cross;
    }

    s16 angle = home.angle.y = (s16)(home.angle.y + 0x2000) & 0xC000;
    shape_angle.y = angle;
    current.angle.y = angle;
    setActionMode(ACTION_HIDE);
}

int daE_HZ_c::create() {
    static dCcD_SrcSph cc_sph_src = {
        {
            {0x0, {{0x0, 0x0, 0x0}, {0x10040, 0x3}, 0x0}},  // mObj
            {dCcD_SE_METAL, 0x0, 0x0, 0x0, 0x0},            // mGObjAt
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},             // mGObjTg
            {0x0},                                          // mGObjCo
        },                                                  // mObjInf
        {
            {{0.0f, 0.0f, 0.0f}, 40.0f}  // mSph
        }  // mSphAttr
    };

    static dCcD_SrcSph cc_sph_src2 = {
        {
            {0x0, {{AT_TYPE_CSTATUE_SWING, 0x1, 0x0}, {0xd8fafdff, 0x3}, 0x75}},   // mObj
            {dCcD_SE_METAL, 0x0, 0x0, 0x0, 0x0},                                        // mGObjAt
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2},                                         // mGObjTg
            {0x0},                                                                      // mGObjCo
        },                                                                              // mObjInf
        {
            {{0.0f, 0.0f, 0.0f}, 40.0f}  // mSph
        }  // mSphAttr
    };

    fopAcM_ct(this, daE_HZ_c);

    dKy_tevstr_init(&mTevStr, home.roomNo, 0xFF);

    int phase = dComIfG_resLoad(&mPhaseReq, "E_HZ");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }

    u8 param = (fopAcM_GetParam(this) >> 8) & 0xFF;
    if (param == 1) {
        int dummy; // force debug to use r31 as stack pointer

        mpName = "E_hzp2";
    } else {
        mpName = "E_hzp";
    }

    phase = dComIfG_resLoad(&mPhaseReq2, mpName);
    if (phase == cPhs_COMPLEATE_e) {
        OS_REPORT("E_HZ PARAM %x\n", fopAcM_GetParam(this));

        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x2C20)) {
            return cPhs_ERROR_e;
        }
        if (mpBgW != NULL && dComIfG_Bgsp().Regist(mpBgW, this)) {
            return cPhs_ERROR_e;
        }

        if (!hio_set) {
            mHIOInit = true;
            hio_set = true;
            l_HIO.enemy_sample = mDoHIO_CREATE_CHILD("ハジキさん", &l_HIO);
        }

        fopAcM_SetMtx(this, mpMorfSO->getModel()->getBaseTRMtx());
        fopAcM_SetMin(this, -200.0f, -200.0f, -200.0f);
        fopAcM_SetMax(this, 200.0f, 200.0f, 200.0f);
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir,
                     fopAcM_GetSpeed_p(this), NULL, NULL);
        mAcchCir.SetWall(50.0f, 150.0f);
        field_0x560 = health = 0x32;
        mStts.Init(0xFF, 0, this);
        mSpheres[0].Set(cc_sph_src);
        mSpheres[1].Set(cc_sph_src2);
        mSpheres[2].Set(cc_sph_src2);
        mSpheres[3].Set(cc_sph_src2);
        mSpheres[0].SetStts(&mStts);
        mSpheres[1].SetStts(&mStts);
        mSpheres[2].SetStts(&mStts);
        mSpheres[3].SetStts(&mStts);
        attention_info.distances[fopAc_attn_BATTLE_e] = 4;
        mSound.init(&current.pos, &eyePos, 3, 1);
        mSound.setEnemyName("E_hz");
        mAtInfo.mpSound = &mSound;
        maxFallSpeed = -60.0f;
        setInitPos();
        mParam = fopAcM_GetParam(this);

        if (mParam != 0xFF && dComIfGs_isSwitch(mParam, fopAcM_GetRoomNo(this))) {
            setActionMode(0xB);
            mMode = 1;
        }
        daE_HZ_Execute(this);
    }
    return phase;
}

static int daE_HZ_Create(daE_HZ_c* i_this) {
    return i_this->create();
}

static DUSK_CONST actor_method_class l_daE_HZ_Method = {
    (process_method_func)daE_HZ_Create,  (process_method_func)daE_HZ_Delete,
    (process_method_func)daE_HZ_Execute, (process_method_func)daE_HZ_IsDelete,
    (process_method_func)daE_HZ_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_HZ = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_HZ_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daE_HZ_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_HZ_e,
    /* Actor SubMtd */ &l_daE_HZ_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

AUDIO_INSTANCES;
