/**
 * @file d_a_e_gi.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_e_gi.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_enemy.h"
#include "f_op/f_op_camera_mng.h"

#if TARGET_PC
#include "dusk/coop/camera.h"
#include "dusk/coop/caught_stun_owner.h"
#include "dusk/coop/damage_owner.h"
#include "dusk/coop/enemy_targeting.h"
#include "dusk/coop/gibdo_state_probe.h"
#include "dusk/coop/input.h"
#include "dusk/coop/selected_target_state.h"
#endif

class daE_GI_HIO_c : public JORReflexible {
public:
    daE_GI_HIO_c();
    virtual ~daE_GI_HIO_c() {}

    void genMessage(JORMContext*);

    /* 0x04 */ s8 id;
    /* 0x08 */ f32 model_size;
    /* 0x0C */ f32 move_speed;
    /* 0x10 */ f32 player_detect_range;
    /* 0x14 */ f32 player_attack_range;
    /* 0x18 */ f32 attack_angle;
    /* 0x1C */ f32 link_stun_time;
    /* 0x20 */ f32 wolf_stun_time;
    /* 0x24 */ f32 scream_prevention_time;
    /* 0x28 */ f32 lever_spin_time;
};

enum daE_GI_ACTION_e {
    ACTION_SLEEP_e,
    ACTION_WAIT_e,
    ACTION_CHASE_e,
    ACTION_ATTACK_e,
    ACTION_DAMAGE_e,
    ATTACK_BITE_DAMAGE_e,
};

namespace {
static dCcD_SrcSph cc_gi_src = {
    {
        {0x0, {{0x0, 0x1, 0x0}, {0xD8FBFDFF, 0x43}, 0x75}}, // mObj
        {dCcD_SE_METAL, 0x0, 0x0, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};

static dCcD_SrcSph cc_gi_att_src = {
    {
        {0x0, {{0x100000, 0x3, 0xD}, {0xD8FBFDFF, 0x0}, 0x75}}, // mObj
        {dCcD_SE_METAL, 0x0, 0x1, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x2, 0x0, 0x0, 0x3}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};
}

daE_GI_HIO_c::daE_GI_HIO_c() {
    id = -1;
    model_size = 1.1f;
    move_speed = 4.0f;
    player_detect_range = 1200.0f;
    player_attack_range = 380.0f;
    attack_angle = 0x4000;
    link_stun_time = 135;
    wolf_stun_time = 110;
    scream_prevention_time = 20;
    lever_spin_time = 35;
}

int daE_GI_c::ctrlJoint(J3DJoint* i_joint, J3DModel* i_model) {
    int jnt_no = i_joint->getJntNo();
    mDoMtx_stack_c::copy(i_model->getAnmMtx(jnt_no));

    switch (jnt_no) {
    case 3:
        mDoMtx_stack_c::XrotM(field_0x67e);
        break;
    }

    i_model->setAnmMtx(jnt_no, mDoMtx_stack_c::get());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    return 1;
}

int daE_GI_c::JointCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        J3DModel* model = j3dSys.getModel();
        daE_GI_c* a_this = (daE_GI_c*)model->getUserArea();
        
        if (a_this != NULL) {
            a_this->ctrlJoint(i_joint, model);
        }
    }

    return 1;
}

int daE_GI_c::draw() {
    J3DModel* model = mpModelMorf->getModel();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(model, &tevStr);
    
    J3DModelData* modelData = model->getModelData();
    J3DMaterial* eyeMaterial = modelData->getMaterialNodePointer(2);
    if (eyeMaterial != NULL) {
        eyeMaterial->getTevColor(2)->r = field_0x6a2;
    }

    if (mBodyDamageColor) {
        J3DMaterial* bodyMaterial = modelData->getMaterialNodePointer(0);
        bodyMaterial->getTevColor(0)->r = mBodyDamageColor;
        bodyMaterial->getTevColor(0)->g = mBodyDamageColor;
        bodyMaterial->getTevColor(0)->b = mBodyDamageColor;

        J3DMaterial* body2Material = modelData->getMaterialNodePointer(1);
        body2Material->getTevColor(0)->r = mBodyDamageColor;
        body2Material->getTevColor(0)->g = mBodyDamageColor;
        body2Material->getTevColor(0)->b = mBodyDamageColor;
    }

    mpModelMorf->entryDL();

    cXyz sp8;
    sp8.set(current.pos.x, 100.0f + current.pos.y, current.pos.z);
    mShadowKey = dComIfGd_setShadow(mShadowKey, 1, model, &sp8, 1200.0f, 0.0f, current.pos.y, mAcch.GetGroundH(), mAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());

    g_env_light.setLightTevColorType_MAJI(mpSwordModel, &tevStr);
    mDoExt_modelUpdateDL(mpSwordModel);
    dComIfGd_addRealShadow(mShadowKey, mpSwordModel);
    return 1;
}

static int daE_GI_Draw(daE_GI_c* a_this) {
    return a_this->draw();
}

void daE_GI_c::setBck(int i_anm, u8 i_mode, f32 i_morf, f32 i_speed) {
#if TARGET_PC
    // Co-op: Gibdo state diagnostics need the current BCK id to distinguish idle, scream,
    // chase, and sword-swing wait states without changing the decompiled animation wrapper.
    dusk::coop::gibdo_state_probe::noteGibdoBck(this, i_anm);
#endif
    mpModelMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_GI", i_anm), i_mode, i_morf, i_speed, 0.0f, -1.0f);
}

void daE_GI_c::setActionMode(int i_actionMode, int i_moveMode) {
    mWallCheckRadius = 150.0f;
    mActionMode = i_actionMode;
    mMoveMode = i_moveMode;
    mIsAttackStart = FALSE;
    offHeadLockFlg();
}

static u8 hio_set;

static daE_GI_HIO_c l_HIO;

#if TARGET_PC
// Co-op: Gibdo's combat state repeatedly asks for player distance/angle/visibility. Keep those
// reads on one Dusk-owned combat target while leaving scream/wolf-bite stun ownership separate.
static bool coOpSelectTargetState(daE_GI_c* i_this, const char* label, bool committed,
                                  dusk::coop::EnemyTargetMode mode,
                                  dusk::coop::selected_target_state::SelectedTargetState* state,
                                  f32* distance, s16* angle_y, s16* angle_x) {
    dusk::coop::EnemyTargetContext context;
    context.observer = i_this;
    context.scope = dusk::coop::EnemyTargetScope::Combat;
    context.mode = mode;
    context.label = label;
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

    if (state != NULL) {
        *state = targetState;
    }
    if (distance != NULL) {
        *distance = target.distance;
    }
    if (angle_y != NULL) {
        *angle_y = target.angleY;
    }
    if (angle_x != NULL) {
        *angle_x = fopAcM_searchActorAngleX(i_this, targetState.actor);
    }
    return true;
}

static dCamera_c* coOpCryCameraForSlot(dusk::coop::PlayerSlot slot) {
    if (slot == dusk::coop::PlayerSlot::Primary) {
        return dCam_getBody();
    }

    if (slot == dusk::coop::PlayerSlot::Secondary && dusk::coop::camera::isSecondaryCameraReady()) {
        camera_process_class* camera = dComIfGp_getCamera(dusk::coop::camera::kSecondaryCameraId);
        return camera != NULL ? &camera->mCamera : NULL;
    }

    return NULL;
}

static void coOpForceCryLockOn(fopAc_ac_c* enemy, dusk::coop::PlayerSlot slot) {
    // Co-op: P2 scream stun may use camera 1, but must not hijack P1's camera if camera 1 is absent.
    dCamera_c* camera = coOpCryCameraForSlot(slot);
    if (camera != NULL) {
        camera->ForceLockOn(enemy);
    }
}

static void coOpForceCryLockOff(fopAc_ac_c* enemy) {
    for (int i = 0; i < 2; i++) {
        dCamera_c* camera = coOpCryCameraForSlot(static_cast<dusk::coop::PlayerSlot>(i));
        if (camera != NULL) {
            camera->ForceLockOff(enemy);
        }
    }
}

static void coOpClearEnemyTargets(daE_GI_c* i_this) {
    dusk::coop::clearAllEnemyTargets(i_this);
}

static f32 coOpScreamAffectedRange() {
    // Co-op: keep vanilla's single scream owner, but let the scream sound affect nearby partners
    // across a wider co-op fight space so we do not need simultaneous Gibdo scream ownership yet.
    return l_HIO.player_detect_range * 1.5f;
}
#endif

void daE_GI_c::damage_check() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    
    if (mInvulnerabilityTimer == 0 && mActionMode != ATTACK_BITE_DAMAGE_e) {
        mCcStts.Move();
        mAtInfo.mpCollider = NULL;

        if (mCcSph[0].ChkTgHit()) {
            mAtInfo.mpCollider = mCcSph[0].GetTgHitObj();
        }

        if (mCcSph[1].ChkTgHit()) {
            mAtInfo.mpCollider = mCcSph[1].GetTgHitObj();
        }

        if (mAtInfo.mpCollider != NULL) {
            mIsOnHeadLock = FALSE;

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD) && ((dCcD_GObjInf*)mAtInfo.mpCollider)->GetAtAtp() >= 4 && ((dCcD_GObjInf*)mAtInfo.mpCollider)->GetAtSpl() == 1) {
                health = 0;
            }

            cc_at_check(this, &mAtInfo);

#if TARGET_PC
            // Co-op: ordinary hit reactions follow the player who struck Gibdo; wolf-bite
            // ownership below remains a caught/stun-owner problem and stays on vanilla P1.
            dusk::coop::damage_owner::DamageOwnerResult damage_owner =
                dusk::coop::damage_owner::resolveDamageOwner(this, mAtInfo.mpCollider);
            daPy_py_c* hit_player =
                dusk::coop::damage_owner::resolveDamageOwnerPlayer(damage_owner);
            if (hit_player == NULL) {
                hit_player = player;
            }
#else
            daPy_py_c* hit_player = player;
#endif

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK | AT_TYPE_WOLF_CUT_TURN | AT_TYPE_10000000 | AT_TYPE_MIDNA_LOCK)) {
                mInvulnerabilityTimer = 20;
            } else {
                mInvulnerabilityTimer = 10;
            }

            if (mAtInfo.mAttackPower <= 1) {
                mInvulnerabilityTimer = KREG_S(8) + 10;
            }

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK) && player->getCutType() != daPy_py_c::CUT_TYPE_WOLF_B_LEFT && player->getCutType() != daPy_py_c::CUT_TYPE_WOLF_B_RIGHT && player->onWolfEnemyHangBite(this)) {
                if (health <= 0) {
                    player->offWolfEnemyHangBite();
                    mSound.startCollisionSE(Z2SE_HIT_WOLFBITE, 0x1F);
                    dComIfGp_getVibration().StartShock(3, 0x1F, cXyz(0.0f, 1.0f, 0.0f));
                    setDamageEffect();
                    setActionMode(ACTION_DAMAGE_e, 10);
                } else {
                    setActionMode(ATTACK_BITE_DAMAGE_e, 0);
                }
                return;
            }

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD)) {
                if (mContinuousHitTimer != 0) {
                    mDamageDirection ^= 1;
                } else if (cM_rnd() < 0.5f) {
                    mDamageDirection = 0;
                } else {
                    mDamageDirection = 1;
                }
                mContinuousHitTimer = 30;
            } else {
                mDamageDirection ^= 1;
            }

            if (health <= 0) {
                setActionMode(ACTION_DAMAGE_e, 10);
            } else {
                BOOL try_cry_stop = FALSE;
                if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD)) {
                    if (cM_rnd() <= 0.15f) {
                        try_cry_stop = TRUE;
                    } else if (hit_player->getCutCount() >= 4 || ((dCcD_GObjInf*)mAtInfo.mpCollider)->GetAtSpl() == 1) {
                        if (cM_rnd() <= 0.25f) {
                            try_cry_stop = TRUE;
                        }
                    }
                } else if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
                    mIsOnHeadLock = TRUE;
                }

                switch (field_0x6a0) {
                case 0:
                    if (try_cry_stop) {
                        if (setCryStop(hit_player)) {
                            setActionMode(ACTION_CHASE_e, 2);
                            return;
                        }

                        setActionMode(ACTION_DAMAGE_e, mDamageDirection);
                        break;
                    }
                    
                    if (hit_player->getCutType() == daPy_py_c::CUT_TYPE_JUMP && hit_player->checkCutJumpCancelTurn()) {
                        mInvulnerabilityTimer = 3;
                    }

                    setActionMode(ACTION_DAMAGE_e, mDamageDirection);
                    break;
                case 3:
                    return;
                }
            }

            if (mPlayerStunTimer != 0) {
                mPlayerStunTimer = 1;
                mCryTimer = l_HIO.scream_prevention_time;
            }
        }
    }
}

void daE_GI_c::setWeaponAtBit(u8 i_onBit) {
    if (!i_onBit) {
        mAtSph[0].OffAtSetBit();
        mAtSph[1].OffAtSetBit();
        mAtSph[2].OffAtSetBit();
        mAtSph[3].OffAtSetBit();
    } else {
        mAtSph[0].OnAtSetBit();
        mAtSph[1].OnAtSetBit();
        mAtSph[2].OnAtSetBit();
        mAtSph[3].OnAtSetBit();
    }   
}

static daE_GI_c* m_cry_gi;

bool daE_GI_c::setCryStop(daPy_py_c* player) {
    if (m_cry_gi == NULL) {
        if (player == NULL) {
            player = daPy_getPlayerActorClass();
        }
#if TARGET_PC
        // Co-op: scream stun is a retained player effect. The selected/damage-owner slot owns
        // release/camera state; nearby slots can share the same scream animation timer.
        dusk::coop::caught_stun_owner::CaughtStunOwnerState stunOwner =
            dusk::coop::caught_stun_owner::beginCaughtStunArea("e_gi.scream", this, player,
                                                               coOpScreamAffectedRange());
        if (stunOwner.localPlayer != NULL) {
            player = stunOwner.localPlayer;
        }
#endif

        if (!player->checkWolf()) {
            mPlayerStunTimer = 9.0f + l_HIO.link_stun_time;
        } else {
            mPlayerStunTimer = 9.0f + l_HIO.wolf_stun_time;
        }

        mPlayerStunTimer += (int)l_HIO.lever_spin_time;
        mPushButtonCount = 0;
        mCryTimer = mPlayerStunTimer + l_HIO.scream_prevention_time;
        m_cry_gi = this;

#if TARGET_PC
        dusk::coop::caught_stun_owner::updateCaughtStun("e_gi.scream", this, mPlayerStunTimer,
                                                        mCryTimer);
        coOpForceCryLockOn(this, stunOwner.slot);
#else
        dCam_getBody()->ForceLockOn(this);
#endif
        speedF = 0.0f;
        setBck(10, 0, 5.0f, 1.0f);
        field_0x6a0 = 3;
        return true;
    }

    return false;
}

void daE_GI_c::setAttackEffect() {
    cXyz size(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
    dComIfGp_particle_setPolyColor(0x8684, mAcch.m_gnd, &current.pos, &tevStr, &shape_angle, &size, 0, NULL, -1, NULL);
    dComIfGp_particle_set(0x8685, &current.pos, &tevStr, &shape_angle, &size);
}

void daE_GI_c::setDragSwordEffect() {
    cXyz size(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
    cXyz pos;

    mDoMtx_stack_c::copy(mpSwordModel->getBaseTRMtx());
    mDoMtx_stack_c::transM(44.0f, -190.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&pos);

    dBgS_GndChk gndchk;
    gndchk.SetPos(&pos);
    
    f32 ground_height = dComIfG_Bgsp().GroundCross(&gndchk);
    if (-G_CM3D_F_INF != ground_height) {
        pos.y = ground_height;
    }

    mPolyColorKey = dComIfGp_particle_setPolyColor(mPolyColorKey, 0x8689, mAcch.m_gnd, &pos, &tevStr, &shape_angle, &size, 0, NULL, -1, NULL);
}

void daE_GI_c::setDeathSmokeEffect() {
    cXyz size(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
    cXyz pos;
    cXyz offset;
    offset.set(0.0f, 0.0f, 85.0f);
    cLib_offsetPos(&pos, &current.pos, shape_angle.y, &offset);
    dComIfGp_particle_setPolyColor(0xE7, mAcch.m_gnd, &pos, &tevStr, &shape_angle, &size, 0, NULL, -1, NULL);
}

void daE_GI_c::setDamageEffect() {
    cXyz size(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
    cXyz pos;
    mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(2));
    mDoMtx_stack_c::multVecZero(&pos);

    for (int i = 0; i < 3; i++) {
        static u16 gi_damage_eff_id[] = {0x8686, 0x8687, 0x8688};
        dComIfGp_particle_set(gi_damage_eff_id[i], &pos, &tevStr, &shape_angle, &size);
    }
}

static void* s_other_gi(void* i_actor, void* i_other) {
    if (i_actor != i_other && fopAcM_IsActor(i_actor) && !fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_E_GI_e &&
        fopAcM_searchActorDistance((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_other) < 1000.0f)
    {
        return i_actor;
    }

    return NULL;
}

static void* s_battle_gi(void* i_actor, void* i_other) {
    if (i_actor != i_other && fopAcM_IsActor(i_actor) && !fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_E_GI_e &&
        ((daE_GI_c*)i_actor)->isBattleOn() && fopAcM_searchActorDistance((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_other) < 500.0f)
    {
        return i_actor;
    }

    return NULL;
}

void daE_GI_c::executeSleep() {
#if TARGET_PC
    // Co-op: wake/turn reads are awareness checks, so they acquire the nearest visible combat
    // target immediately instead of inheriting stale chase stickiness.
    dusk::coop::selected_target_state::SelectedTargetState targetState;
    fopAc_ac_c* target_actor = daPy_getPlayerActorClass();
    f32 player_dist = fopAcM_searchPlayerDistance(this);
    s16 player_angle = fopAcM_searchPlayerAngleY(this);
    if (coOpSelectTargetState(this, "e_gi.sleep", false,
                              dusk::coop::EnemyTargetMode::ImmediateAcquire, &targetState,
                              &player_dist, &player_angle, NULL))
    {
        target_actor = targetState.actor;
    }
#else
    fopAc_ac_c* target_actor = daPy_getPlayerActorClass();
    f32 player_dist = fopAcM_searchPlayerDistance(this);
    s16 player_angle = fopAcM_searchPlayerAngleY(this);
#endif

    switch (mMoveMode) {
    case 0:
        field_0x6a0 = 0;
        setBck(9, 0, 3.0f, 1.0f);
        mpModelMorf->setFrame(1.0f);
        mpModelMorf->setPlaySpeed(0.0f);
        mMoveMode = 1;
        mCcSph[0].OffTgSetBit();
        mCcSph[1].OffTgSetBit();
        /* fallthrough */
    case 1:
        attention_info.flags = 0;

        if (mSwbit2 != 0xFF) {
            if (dComIfGs_isSwitch(mSwbit2, fopAcM_GetRoomNo(this))) {
                mpModelMorf->setPlaySpeed(1.0f);
                mMoveMode = 2;
            }
        } else {
            if (player_dist < l_HIO.player_detect_range && !fopAcM_otherBgCheck(this, target_actor)) {
                mpModelMorf->setPlaySpeed(1.0f);
                mMoveMode = 2;
            }
        }
        break;
    case 2:
        if (mpModelMorf->isStop()) {
            setBck(4, 0, 3.0f, 1.0f);
            mMoveMode = 3;
            field_0x668 = cM_rndFX(0.2f);
            field_0x66c = 1024.0f + cM_rndFX(256.0f);
            mCcSph[0].OnTgSetBit();
            mCcSph[1].OnTgSetBit();
        }
        break;
    case 3:
        if (mpModelMorf->getFrame() >= 14.0f) {
            if (mpModelMorf->getFrame() >= 40.0f) {
                cLib_chaseF(&speedF, 0.0f, 0.5f);
            } else {
                cLib_chaseF(&speedF, 2.0f, 0.5f);
            }
            cLib_addCalcAngleS(&shape_angle.y, player_angle, 0x10, field_0x66c, 0x40);
            current.angle.y = shape_angle.y;
        }

        if (mpModelMorf->isStop()) {
            if (player_dist > l_HIO.player_detect_range) {
                setActionMode(ACTION_WAIT_e, 0);
            } else {
                setActionMode(ACTION_CHASE_e, 10);
            }
        }
        break;
    }
}

void daE_GI_c::executeWait() {
    f32 player_dist = fopAcM_searchPlayerDistance(this);
    s16 player_angle = fopAcM_searchPlayerAngleY(this);
    fopAc_ac_c* target_actor = daPy_getPlayerActorClass();
    daPy_py_c* cry_player = daPy_getPlayerActorClass();
#if TARGET_PC
    // Co-op: idle awareness may wake on any active player; if scream stun starts, bind it to
    // that selected target through caught/stun ownership.
    dusk::coop::selected_target_state::SelectedTargetState targetState;
    if (coOpSelectTargetState(this, "e_gi.wait", false,
                              dusk::coop::EnemyTargetMode::ImmediateAcquire, &targetState,
                              &player_dist, &player_angle, NULL))
    {
        target_actor = targetState.actor;
        cry_player = targetState.player;
    }
#endif

    switch (mMoveMode) {
    case 0:
        field_0x6a0 = 0;
        speedF = 0.0f;
        setBck(11, 2, 10.0f, 1.0f);
        mMoveMode = 1;
    case 1:
        if ((mSwbit2 == 0xFF || dComIfGs_isSwitch(mSwbit2, fopAcM_GetRoomNo(this))) && player_dist < l_HIO.player_detect_range) {
            const bool angle_gate = abs((s16)(player_angle - shape_angle.y)) < (s16)l_HIO.attack_angle;
            const bool los_clear = !fopAcM_otherBgCheck(this, target_actor);
#if TARGET_PC
            {
                dusk::coop::gibdo_state_probe::GibdoStateProbe probe;
                probe.actor = reinterpret_cast<uintptr_t>(this);
                probe.actorId = fopAcM_GetID(this);
                probe.action = mActionMode;
                probe.moveMode = mMoveMode;
                probe.bck = dusk::coop::gibdo_state_probe::currentGibdoBck(this);
                probe.animFrame = mpModelMorf->getFrame();
                probe.playSpeed = mpModelMorf->getPlaySpeed();
                probe.speedF = speedF;
                probe.attackDelay = field_0x684;
                probe.stunTimer = mPlayerStunTimer;
                probe.cryTimer = mCryTimer;
                probe.targetSlot = targetState.slot;
                probe.targetFound = targetState.available;
                probe.targetDistance = player_dist;
                probe.targetAngleY = player_angle;
                probe.rangeGate = true;
                probe.angleGate = angle_gate;
                probe.losClear = los_clear;
                probe.delayGate = true;
                probe.attackGate = angle_gate && los_clear;
                probe.attackStart = mIsAttackStart;
                probe.screamOwnerActive = m_cry_gi != NULL;
                probe.screamOwnerAttackStarted = m_cry_gi != NULL && m_cry_gi->isAttackStart();
                probe.cryOwner = reinterpret_cast<uintptr_t>(m_cry_gi);
                probe.label = "e_gi.wait";
                // Co-op: diagnostics only. This records why an awake Gibdo did or did not leave
                // wait state for a selected co-op player without altering vanilla gates.
                dusk::coop::gibdo_state_probe::recordGibdoStateProbe(probe);
            }
#endif
            if (angle_gate && los_clear) {
                if (player_dist < l_HIO.player_attack_range && setCryStop(cry_player)) {
                    setActionMode(ACTION_CHASE_e, 2);
                } else {
                    setActionMode(ACTION_CHASE_e, 0);
                }
                return;
            }

            if (fpcM_Search(s_battle_gi, this)) {
                setActionMode(ACTION_CHASE_e, 0);
            }
        }
    }
}

void daE_GI_c::executeChase() {
    field_0x698 = 1;
    f32 player_dist = fopAcM_searchPlayerDistance(this);
    s16 player_angle = fopAcM_searchPlayerAngleY(this);
    fopAc_ac_c* target_actor = daPy_getPlayerActorClass();
    daPy_py_c* cry_player = daPy_getPlayerActorClass();
#if TARGET_PC
    // Co-op: chase/attack gates use sticky combat target state so movement, distance checks,
    // and attack setup agree on the same player until combat retention permits a switch.
    dusk::coop::selected_target_state::SelectedTargetState targetState;
    if (coOpSelectTargetState(this, "e_gi.chase", false,
                              dusk::coop::EnemyTargetMode::StickyCombat, &targetState,
                              &player_dist, &player_angle, NULL))
    {
        target_actor = targetState.actor;
        cry_player = targetState.player;
    }
#endif

    switch (mMoveMode) {
    case 0:
    case 10:
        field_0x6a0 = 0;

        if (mMoveMode == 10) {
            setBck(12, 2, 10.0f, 1.0f);
        } else {
            setBck(12, 2, 3.0f, 1.0f);
        }

        mMoveMode = 1;
        field_0x668 = cM_rndFX(0.2f);
        field_0x66c = 1024.0f + cM_rndFX(256.0f);
        field_0x684 = 30;

        if (player_dist < 50.0f + l_HIO.player_attack_range && abs((s16)(player_angle - shape_angle.y)) < 0x2800 && !fopAcM_otherBgCheck(this, target_actor)) {
            field_0x684 = 0;
        }
    case 1: {
        setDragSwordEffect();

        if (mpModelMorf->checkFrame(10.0f)) {
            mSound.startCreatureSound(Z2SE_EN_GI_WALK, 0, -1);
        }

        f32 anm_frame = mpModelMorf->getFrame();
        if (anm_frame >= 10.0f && anm_frame < 29.0f) {
            cLib_chaseF(&speedF, (field_0x668 + l_HIO.move_speed) - 1.0f, 0.5f);
        } else if (anm_frame >= 34.0f && anm_frame < 54.0f) {
            cLib_chaseF(&speedF, field_0x668 + l_HIO.move_speed, 0.5f);
        } else {
            cLib_chaseF(&speedF, 0.5f, 0.5f);
        }

        cLib_addCalcAngleS(&shape_angle.y, player_angle, 0x10, field_0x66c, 0x40);
        current.angle.y = shape_angle.y;

#if TARGET_PC
        // Co-op: chase remains sticky, but the moment-to-moment attack/scream gate should ask
        // which player is actually close enough now so stale retention cannot suppress a valid hit.
        coOpSelectTargetState(this, "e_gi.attack_gate", false,
                              dusk::coop::EnemyTargetMode::ImmediateAcquire, &targetState,
                              &player_dist, &player_angle, NULL);
        target_actor = targetState.available ? targetState.actor : target_actor;
        cry_player = targetState.available ? targetState.player : cry_player;
#endif

        if (player_dist < (50.0f + l_HIO.player_attack_range)) {
            const bool angle_gate = abs((s16)(player_angle - shape_angle.y)) < 0x2800;
            const bool los_clear = !fopAcM_otherBgCheck(this, target_actor);
            const bool delay_gate = field_0x684 == 0;
#if TARGET_PC
            {
                dusk::coop::gibdo_state_probe::GibdoStateProbe probe;
                probe.actor = reinterpret_cast<uintptr_t>(this);
                probe.actorId = fopAcM_GetID(this);
                probe.action = mActionMode;
                probe.moveMode = mMoveMode;
                probe.bck = dusk::coop::gibdo_state_probe::currentGibdoBck(this);
                probe.animFrame = mpModelMorf->getFrame();
                probe.playSpeed = mpModelMorf->getPlaySpeed();
                probe.speedF = speedF;
                probe.attackDelay = field_0x684;
                probe.stunTimer = mPlayerStunTimer;
                probe.cryTimer = mCryTimer;
                probe.targetSlot = targetState.slot;
                probe.targetFound = targetState.available;
                probe.targetDistance = player_dist;
                probe.targetAngleY = player_angle;
                probe.rangeGate = true;
                probe.angleGate = angle_gate;
                probe.losClear = los_clear;
                probe.delayGate = delay_gate;
                probe.attackGate = angle_gate && los_clear && delay_gate;
                probe.attackStart = mIsAttackStart;
                probe.screamOwnerActive = m_cry_gi != NULL;
                probe.screamOwnerAttackStarted = m_cry_gi != NULL && m_cry_gi->isAttackStart();
                probe.cryOwner = reinterpret_cast<uintptr_t>(m_cry_gi);
                probe.label = "e_gi.attack_gate";
                // Co-op: diagnostics only. This captures the exact close-range gate so a
                // P2-first wake stall can be traced to range, angle, line of sight, delay, or scream lock.
                dusk::coop::gibdo_state_probe::recordGibdoStateProbe(probe);
            }
#endif
            if (angle_gate && los_clear && delay_gate) {
                if (setCryStop(cry_player)) {
                    mMoveMode = 2;
                } else {
                    setActionMode(ACTION_ATTACK_e, 5);
                    setBck(11, 2, 10.0f, 1.0f);
                    speedF = 0.0f;
                }
            }
        } else if (player_dist > (200.0f + l_HIO.player_detect_range)) {
            setActionMode(ACTION_WAIT_e, 0);
        }
        break;
    }
    case 2:
        field_0x69f = 1;

        if (mpModelMorf->checkFrame(10.0f)) {
            mSound.startCreatureSound(Z2SE_EN_GI_WALK, 0, -1);
            mSound.startCreatureSound(Z2SE_EN_GI_SHOUT, 0, -1);
            field_0x6a0 = 0;
        }

        if (mpModelMorf->isStop()) {
            setActionMode(ACTION_ATTACK_e, 0);
            mIsAttackStart = TRUE;
        }
        break;
    }
}

void daE_GI_c::executeAttack() {
    field_0x698 = 1;
    s16 player_angle = fopAcM_searchPlayerAngleY(this);
#if TARGET_PC
    // Co-op: attack follow-through keeps the same committed combat target through the swing.
    dusk::coop::selected_target_state::SelectedTargetState targetState;
    coOpSelectTargetState(this, "e_gi.attack", true, dusk::coop::EnemyTargetMode::StickyCombat,
                          &targetState, NULL, &player_angle, NULL);
#endif

    switch (mMoveMode) {
    case 0:
        field_0x6a0 = 0;
        speedF = 0.0f;
        setBck(5, 0, 10.0f, 1.0f);
        mMoveMode = 1;
        /* fallthrough */
    case 1:
#if TARGET_PC
        {
            dusk::coop::gibdo_state_probe::GibdoStateProbe probe;
            probe.actor = reinterpret_cast<uintptr_t>(this);
            probe.actorId = fopAcM_GetID(this);
            probe.action = mActionMode;
            probe.moveMode = mMoveMode;
            probe.bck = dusk::coop::gibdo_state_probe::currentGibdoBck(this);
            probe.animFrame = mpModelMorf->getFrame();
            probe.playSpeed = mpModelMorf->getPlaySpeed();
            probe.speedF = speedF;
            probe.attackDelay = field_0x684;
            probe.stunTimer = mPlayerStunTimer;
            probe.cryTimer = mCryTimer;
            probe.targetSlot = targetState.slot;
            probe.targetFound = targetState.available;
            probe.targetAngleY = player_angle;
            probe.rangeGate = true;
            probe.angleGate = true;
            probe.losClear = true;
            probe.delayGate = true;
            probe.attackGate = true;
            probe.attackStart = true;
            probe.screamOwnerActive = m_cry_gi != NULL;
            probe.screamOwnerAttackStarted = m_cry_gi != NULL && m_cry_gi->isAttackStart();
            probe.cryOwner = reinterpret_cast<uintptr_t>(m_cry_gi);
            probe.label = "e_gi.attack_swing";
            // Co-op: diagnostics only. A real sword swing has distinct animation progress, so this
            // separates genuine attacks from pre-swing attack wait states.
            dusk::coop::gibdo_state_probe::recordGibdoStateProbe(probe);
        }
#endif
        if (mpModelMorf->checkFrame(60.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_GI_V_ATK, -1);
        }

        if (mpModelMorf->checkFrame(71.0f)) {
            mSound.startCreatureSound(Z2SE_EN_GI_ATK_STRK, 0, -1);
        }

        if (mpModelMorf->checkFrame(76.0f)) {
            setAttackEffect();
            mSound.startCreatureSound(Z2SE_EN_GI_ATK_IMPCT, 0, -1);
        }

        if (mpModelMorf->getFrame() < 70.0f) {
            cLib_chaseF(&mWallCheckRadius, 200.0f, 1.5f);
            cLib_addCalcAngleS(&shape_angle.y, (player_angle + 0x400), 0x10, 0x200, 0x80);
            current.angle.y = shape_angle.y;
        } else if (mpModelMorf->getFrame() <= 80.0f) {
            setWeaponAtBit(1);
            cXyz start;
            cXyz end;
            mDoMtx_stack_c::copy(mpSwordModel->getBaseTRMtx());
            mDoMtx_stack_c::multVecZero(&start);
            mDoMtx_stack_c::transM(33.0f, -150.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&end);
            
            dBgS_LinChk linecheck;
            linecheck.Set(&start, &end, this);
            if (dComIfG_Bgsp().LineCross(&linecheck)) {
                dComIfGp_setHitMark(2, this, &linecheck.GetCross(), NULL, NULL, 0);
                mpModelMorf->setPlaySpeed(-1.0f);
                mMoveMode = 2;
                return;
            }
        }

        if (mpModelMorf->checkFrame(60.0f)) {
            field_0x6a0 = 3;
        }

        if (mpModelMorf->checkFrame(80.0f)) {
            field_0x6a0 = 0;
        }

        if (mpModelMorf->isStop()) {
            setActionMode(ACTION_CHASE_e, 10);
        }
        break;
    case 2:
        if (mpModelMorf->checkFrame(1.0f)) {
            setActionMode(ACTION_CHASE_e, 10);
        }
        break;
    case 5:
        speedF = 0.0f;
        setBck(11, 2, 10.0f, 1.0f);
        mMoveMode = 6;
        /* fallthrough */
    case 6:
#if TARGET_PC
        {
            dusk::coop::gibdo_state_probe::GibdoStateProbe probe;
            probe.actor = reinterpret_cast<uintptr_t>(this);
            probe.actorId = fopAcM_GetID(this);
            probe.action = mActionMode;
            probe.moveMode = mMoveMode;
            probe.bck = dusk::coop::gibdo_state_probe::currentGibdoBck(this);
            probe.animFrame = mpModelMorf->getFrame();
            probe.playSpeed = mpModelMorf->getPlaySpeed();
            probe.speedF = speedF;
            probe.attackDelay = field_0x684;
            probe.stunTimer = mPlayerStunTimer;
            probe.cryTimer = mCryTimer;
            probe.targetSlot = targetState.slot;
            probe.targetFound = targetState.available;
            probe.targetAngleY = player_angle;
            probe.rangeGate = true;
            probe.angleGate = true;
            probe.losClear = true;
            probe.delayGate = m_cry_gi == NULL || m_cry_gi->isAttackStart();
            probe.attackGate = probe.delayGate;
            probe.attackStart = mIsAttackStart;
            probe.screamOwnerActive = m_cry_gi != NULL;
            probe.screamOwnerAttackStarted = m_cry_gi != NULL && m_cry_gi->isAttackStart();
            probe.cryOwner = reinterpret_cast<uintptr_t>(m_cry_gi);
            probe.label = "e_gi.attack_wait";
            // Co-op: diagnostics only. Gibdo can enter attack mode before a sword swing; this
            // records whether the native scream coordinator is preventing that transition.
            dusk::coop::gibdo_state_probe::recordGibdoStateProbe(probe);
        }
#endif
        if (m_cry_gi == NULL) {
            field_0x684 = cM_rndF(20.0f);
            mMoveMode = 7;
        } else if (m_cry_gi->isAttackStart()) {
            field_0x684 = 10.0f + cM_rndF(40.0f);
            mMoveMode = 7;
        }
        break;
    case 7:
#if TARGET_PC
        {
            dusk::coop::gibdo_state_probe::GibdoStateProbe probe;
            probe.actor = reinterpret_cast<uintptr_t>(this);
            probe.actorId = fopAcM_GetID(this);
            probe.action = mActionMode;
            probe.moveMode = mMoveMode;
            probe.bck = dusk::coop::gibdo_state_probe::currentGibdoBck(this);
            probe.animFrame = mpModelMorf->getFrame();
            probe.playSpeed = mpModelMorf->getPlaySpeed();
            probe.speedF = speedF;
            probe.attackDelay = field_0x684;
            probe.stunTimer = mPlayerStunTimer;
            probe.cryTimer = mCryTimer;
            probe.targetSlot = targetState.slot;
            probe.targetFound = targetState.available;
            probe.targetAngleY = player_angle;
            probe.rangeGate = true;
            probe.angleGate = true;
            probe.losClear = true;
            probe.delayGate = field_0x684 == 0;
            probe.attackGate = field_0x684 == 0;
            probe.attackStart = mIsAttackStart;
            probe.screamOwnerActive = m_cry_gi != NULL;
            probe.screamOwnerAttackStarted = m_cry_gi != NULL && m_cry_gi->isAttackStart();
            probe.cryOwner = reinterpret_cast<uintptr_t>(m_cry_gi);
            probe.label = "e_gi.attack_delay";
            // Co-op: diagnostics only. This countdown must reach zero before the real sword
            // animation starts; if it loops, the probe will show that delay rather than targeting.
            dusk::coop::gibdo_state_probe::recordGibdoStateProbe(probe);
        }
#endif
        if (field_0x684 == 0) {
            mMoveMode = 0;
        }
        break;
    }
}

void daE_GI_c::executeDamage() {
    f32 player_dist = fopAcM_searchPlayerDistance(this);
#if TARGET_PC
    // Co-op: damage recovery re-entry should test the retained combat target, not always P1.
    coOpSelectTargetState(this, "e_gi.damage", false,
                          dusk::coop::EnemyTargetMode::StickyCombat, NULL, &player_dist, NULL,
                          NULL);
#endif

    switch (mMoveMode) {
    case 0:
    case 1:
        mSound.startCreatureVoice(Z2SE_EN_GI_V_DMG, -1);
        setDamageEffect();
        speedF = 0.0f;
        field_0x6a0 = 0;

        if (mMoveMode == 0) {
            setBck(7, 0, 3.0f, 1.0f);
        } else {
            setBck(6, 0, 3.0f, 1.0f);
        }

        mMoveMode = 2;
        return;
    case 2:
        if (mIsOnHeadLock) {
            onHeadLockFlg();
        }

        if (mpModelMorf->isStop()) {
            if (player_dist < l_HIO.player_attack_range && cM_rnd() < 0.5 && !fpcM_Search(s_other_gi, this)) {
                setActionMode(ACTION_ATTACK_e, 0);
            } else {
                setActionMode(ACTION_CHASE_e, 0);
            }
        }
        break;
    case 10:
        mSound.startCreatureVoice(Z2SE_EN_GI_V_DEAD, -1);

        mCcSph[0].OffTgSetBit();
        mCcSph[1].OffTgSetBit();
        mCcSph[0].ClrTgHit();
        mCcSph[1].ClrTgHit();
        mCcStts.Init(255, 0, this);

        speedF = 0.0f;
        setBck(8, 0, 3.0f, 1.0f);
        mMoveMode = 11;

        if (m_cry_gi == this) {
#if TARGET_PC
            // Co-op: death also releases the retained scream owner/camera slot.
            coOpForceCryLockOff(this);
            dusk::coop::caught_stun_owner::clearCaughtStun("e_gi.scream", this);
#else
            dCam_getBody()->ForceLockOff(this);
#endif
            m_cry_gi = NULL;
        }
        break;
    case 11:
        attention_info.flags = 0;
        cLib_chaseF(&mWallCheckRadius, 200.0f, 1.5f);

        if (mpModelMorf->checkFrame(72.0f)) {
            setDeathSmokeEffect();
        }

        if (mpModelMorf->getFrame() >= 45.0f) {
            cLib_chaseF(&mBodyDamageColor, -50.0f, 0.5f);
        }

        if (mpModelMorf->isStop()) {
            if (mSwbit != 0xFF) {
                dComIfGs_onSwitch(mSwbit, fopAcM_GetRoomNo(this));
            }
            fopAcM_createDisappear(this, &current.pos, 10, 0, 0x22);
            fopAcM_delete(this);
        }
        break;
    }
}

void daE_GI_c::executeBiteDamage() {
    daPy_py_c* player = daPy_getPlayerActorClass();

    switch (mMoveMode) {
    case 0:
        mSound.startCollisionSE(Z2SE_HIT_WOLFBITE, 0x1F);
        setBck(14, 0, 3.0f, 1.0f);
        mMoveMode = 1;
        mWolfBiteCount = 0;
        dComIfGp_getVibration().StartShock(3, 0x1F, cXyz(0.0f, 1.0f, 0.0f));
        setDamageEffect();
        speedF = 0.0f;
        mPlayerStunTimer = 0;
        /* fallthrough */
    case 1:
        player->setWolfEnemyHangBiteAngle((shape_angle.y - 0x2000));

        if (mpModelMorf->isStop()) {
            setBck(16, 2, 3.0f, 1.0f);
        }

        if (checkWolfBiteDamage()) {
            dComIfGp_getVibration().StartShock(3, 0x1F, cXyz(0.0f, 1.0f, 0.0f));
            offWolfBiteDamage();
            mWolfBiteCount++;

            health -= 20;
            if (health < 0) {
                player->offWolfEnemyHangBite();
                mSound.startCollisionSE(Z2SE_HIT_WOLFBITE, 0x20);
                setDamageEffect();
                setActionMode(ACTION_DAMAGE_e, 10);
            } else if (mWolfBiteCount >= 5) {
                setDamageEffect();
                player->offWolfEnemyHangBite();
                mSound.startCollisionSE(Z2SE_HIT_WOLFBITE, 0x1F);
                setBck(15, 0, 3.0f, 1.0f);
                setActionMode(ACTION_DAMAGE_e, 2);
            } else {
                setBck(13, 0, 3.0f, 1.0f);
                mSound.startCollisionSE(Z2SE_HIT_WOLFBITE, 0x1E);
            }
        }

        if (!player->checkWolfEnemyHangBiteOwn(this)) {
            player->offWolfEnemyHangBite();
            offWolfBiteDamage();
            setBck(15, 0, 3.0f, 1.0f);
            setActionMode(ACTION_DAMAGE_e, 2);
        }
        break;
    }
}

void daE_GI_c::PushButtonCount() {
    if (mPlayerStunTimer != 0) {
#if TARGET_PC
        // Co-op: scream release input belongs to the retained stunned slot, not always PAD_1.
        dusk::coop::caught_stun_owner::CaughtStunOwnerState stunOwner =
            dusk::coop::caught_stun_owner::updateCaughtStun("e_gi.scream", this,
                                                            mPlayerStunTimer, mCryTimer);
        if (!stunOwner.found) {
            stunOwner = dusk::coop::caught_stun_owner::beginCaughtStun(
                "e_gi.scream", this, daPy_getPlayerActorClass());
        }
        dusk::coop::PlayerInputState input = dusk::coop::readLocalInput(stunOwner.slot);
        if (abs((s16)(mPrevStickAngle - input.stickAngle3D)) > 0x1000) {
            mPushButtonCount++;
        }

        if (input.triggerButtons & PAD_BUTTON_A) {
            mPushButtonCount += 2;
        }

        if (input.triggerButtons & PAD_BUTTON_B) {
            mPushButtonCount += 2;
        }

        if (input.triggerButtons & PAD_TRIGGER_L) {
            mPushButtonCount += 2;
        }

        if (input.triggerButtons & PAD_TRIGGER_R) {
            mPushButtonCount += 2;
        }
#else
        if (abs((s16)(mPrevStickAngle - mDoCPd_c::getStickAngle3D(PAD_1))) > 0x1000) {
            mPushButtonCount++;
        }
    
        if (mDoCPd_c::getTrigA(PAD_1)) {
            mPushButtonCount += 2;
        }

        if (mDoCPd_c::getTrigB(PAD_1)) {
            mPushButtonCount += 2;
        }

        if (mDoCPd_c::getTrigL(PAD_1)) {
            mPushButtonCount += 2;
        }

        if (mDoCPd_c::getTrigR(PAD_1)) {
            mPushButtonCount += 2;
        }
#endif

        mPlayerStunTimer -= mPushButtonCount / 2;
        if (mPlayerStunTimer < 0) {
            mPlayerStunTimer = 0;
        }

        mCryTimer = mPlayerStunTimer + l_HIO.scream_prevention_time;
        mPushButtonCount &= 1;
    }

#if TARGET_PC
    dusk::coop::caught_stun_owner::CaughtStunOwnerState stunOwner =
        dusk::coop::caught_stun_owner::getCaughtStun(this);
    if (stunOwner.found) {
        mPrevStickAngle = dusk::coop::readLocalInput(stunOwner.slot).stickAngle3D;
    } else {
        mPrevStickAngle = mDoCPd_c::getStickAngle3D(PAD_1);
    }
#else
    mPrevStickAngle = mDoCPd_c::getStickAngle3D(PAD_1);
#endif
}

void daE_GI_c::action() {
    setWeaponAtBit(0);
    field_0x69f = 0;
    field_0x698 = 0;

    damage_check();

    u8 is_battle_on = FALSE;
    attention_info.flags = fopAc_AttnFlag_BATTLE_e;

    switch (mActionMode) {
    case ACTION_SLEEP_e:
        executeSleep();
        break;
    case ACTION_WAIT_e:
        executeWait();
        break;
    case ACTION_CHASE_e:
        is_battle_on = TRUE;
        executeChase();
        break;
    case ACTION_ATTACK_e:
        is_battle_on = TRUE;
        executeAttack();
        break;
    case ACTION_DAMAGE_e:
        is_battle_on = TRUE;
        executeDamage();
        break;
    case ATTACK_BITE_DAMAGE_e:
        is_battle_on = TRUE;
        executeBiteDamage();
        break;
    }

    mAcchCir.SetWall(20.0f, mWallCheckRadius);
    mIsBattleOn = is_battle_on;
    mSound.setLinkSearch(is_battle_on);

    if (attention_info.flags & fopAc_AttnFlag_BATTLE_e) {
        dBgS_LinChk linecheck;
        linecheck.Set(&dComIfGp_getCamera(0)->view.lookat.eye, &attention_info.position, this);
        if (dComIfG_Bgsp().LineCross(&linecheck)) {
            attention_info.flags &= ~fopAc_AttnFlag_BATTLE_e;
        }
    }

    PushButtonCount();

    if (field_0x698 != 0) {
        s16 target_angle = fopAcM_searchPlayerAngleY(this);
#if TARGET_PC
        // Co-op: the head aim follows the selected combat target so visual attention matches
        // chase/attack ownership instead of snapping back to P1.
        coOpSelectTargetState(this, "e_gi.look", false,
                              dusk::coop::EnemyTargetMode::StickyCombat, NULL, NULL,
                              &target_angle, NULL);
#endif
        s16 var_r28 = target_angle - shape_angle.y;
        if (var_r28 > 0x2000) {
            var_r28 = 0x2000;
        }
        if (var_r28 < -0x2000) {
            var_r28 = -0x2000;
        }
        cLib_addCalcAngleS(&field_0x67e, var_r28, 0x10, 0x400, 0x80);
    } else {
        cLib_addCalcAngleS(&field_0x67e, 0, 0x10, 0x400, 0x80);
    }

    if (field_0x69f != 0) {
        cLib_chaseS(&field_0x6a2, 0xFF, 0x10);
    } else {
        cLib_chaseS(&field_0x6a2, 0, 0x10);
    }

    fopAcM_posMoveF(this, mCcStts.GetCCMoveP());

    cXyz down_pos;
    mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(4));
    mDoMtx_stack_c::multVecZero(&down_pos);
    setDownPos(&down_pos);

    cXyz lock_pos(eyePos);
    lock_pos.y += 300.0f;
    setHeadLockPos(&lock_pos);

    mAcch.CrrPos(dComIfG_Bgsp());
    mpModelMorf->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

void daE_GI_c::mtx_set() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mDoMtx_stack_c::scaleM(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);

    J3DModel* model = mpModelMorf->getModel();
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    mpModelMorf->modelCalc();
    mpSwordModel->setBaseTRMtx(model->getAnmMtx(16));
}

void daE_GI_c::cc_set() {
    cXyz cc_center;
    J3DModel* model = mpModelMorf->getModel();

    mDoMtx_stack_c::copy(model->getAnmMtx(4));
    mDoMtx_stack_c::transM(0.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&eyePos);
    attention_info.position = eyePos;
    attention_info.position.y += 30.0f;

    for (int i = 0; i < 2; i++) {
        if (i == 0) {
            mDoMtx_stack_c::copy(model->getAnmMtx(1));
            mDoMtx_stack_c::transM(-33.0f, 0.0f, 0.0f);
        } else {
            mDoMtx_stack_c::copy(model->getAnmMtx(3));
            mDoMtx_stack_c::transM(-11.0f, 0.0f, 0.0f);
        }

        mDoMtx_stack_c::multVecZero(&cc_center);
        mCcSph[i].SetC(cc_center);
        mCcSph[i].SetR(55.0f);
        dComIfG_Ccsp()->Set(&mCcSph[i]);
    }

    mDoMtx_stack_c::copy(mpSwordModel->getBaseTRMtx());
    mDoMtx_stack_c::transM(0.0f, 33.0f, 0.0f);

    for (int i = 0; i < 4; i++) {
        mDoMtx_stack_c::transM(11.0f, -55.0f, 0.0f);
        mDoMtx_stack_c::multVecZero(&cc_center);
        mAtSph[i].SetC(cc_center);
        mAtSph[i].SetR(33.0f);
        dComIfG_Ccsp()->Set(&mAtSph[i]);
    }
}

int daE_GI_c::execute() {
    if (field_0x684 != 0) {
        field_0x684--;
    }

    if (field_0x688 != 0) {
        field_0x688--;
    }

    if (mInvulnerabilityTimer != 0) {
        mInvulnerabilityTimer--;
    }

    if (mPlayerStunTimer != 0) {
        mPlayerStunTimer--;

#if TARGET_PC
        // Co-op: the scream owner remains the release/camera slot, but the sound itself can also
        // paralyze nearby active players for the same vanilla Gibdo timer.
        dusk::coop::caught_stun_owner::CaughtStunOwnerState stunOwner =
            dusk::coop::caught_stun_owner::updateCaughtStun("e_gi.scream", this,
                                                            mPlayerStunTimer, mCryTimer);
        for (int i = 0; i < stunOwner.affectedCount; i++) {
            daPy_py_c* stun_player =
                static_cast<daPy_py_c*>(stunOwner.affectedLocalActors[i]);
            if (stun_player == NULL || stun_player->getDamageWaitTimer() >= 30) {
                continue;
            }

            if (!stun_player->checkWolf()) {
                if (mPlayerStunTimer < (l_HIO.link_stun_time + l_HIO.lever_spin_time)) {
                    stun_player->onNsScreamAnm();
                }
            } else if (mPlayerStunTimer < (l_HIO.wolf_stun_time + l_HIO.lever_spin_time)) {
                stun_player->onNsScreamAnm();
            }
        }
#else
        daPy_py_c* stun_player = daPy_getPlayerActorClass();

        if (stun_player->getDamageWaitTimer() < 30) {
            if (!stun_player->checkWolf()) {
                if (mPlayerStunTimer < (l_HIO.link_stun_time + l_HIO.lever_spin_time)) {
                    stun_player->onNsScreamAnm();
                }
            } else if (mPlayerStunTimer < (l_HIO.wolf_stun_time + l_HIO.lever_spin_time)) {
                stun_player->onNsScreamAnm();
            }
        }
#endif
    }

    if (mContinuousHitTimer != 0) {
        mContinuousHitTimer--;
    }

    if (mCryTimer != 0) {
        mCryTimer--;
        if (mCryTimer == 0 && m_cry_gi == this) {
#if TARGET_PC
            // Co-op: release whichever camera slot the retained scream owner used.
            coOpForceCryLockOff(this);
            dusk::coop::caught_stun_owner::clearCaughtStun("e_gi.scream", this);
#else
            dCam_getBody()->ForceLockOff(this);
#endif
            m_cry_gi = NULL;
        }
    }

    action();
    mtx_set();
    cc_set();

    cXyz effpos;
    mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(2));
    mDoMtx_stack_c::transM(-30.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&effpos);
    cXyz effsize(1.0f, 1.5f, 1.0f);
    setMidnaBindEffect(this, &mSound, &effpos, &effsize);
    return 1;
}

static int daE_GI_Execute(daE_GI_c* a_this) {
    return a_this->execute();
}

static int daE_GI_IsDelete(daE_GI_c* a_this) {
    return 1;
}

int daE_GI_c::_delete() {
#if TARGET_PC
    // Co-op: remove retained targeting/debug state before this actor pointer can be reused.
    coOpClearEnemyTargets(this);
    if (m_cry_gi == this) {
        // Co-op: the global scream owner blocks future screams; if this owner unloads before its
        // own cry timer clears, release the sidecar/camera state and restore the native null owner.
        coOpForceCryLockOff(this);
        m_cry_gi = NULL;
    }
    dusk::coop::caught_stun_owner::clearCaughtStun("e_gi.scream", this);
    dusk::coop::gibdo_state_probe::clearGibdoStateProbe(this);
#endif

    dComIfG_resDelete(&mPhase, "E_GI");

    if (mHIOInit) {
        hio_set = FALSE;
        mDoHIO_DELETE_CHILD(l_HIO.id);
    }

    if (heap != NULL) {
        mSound.deleteObject();
    }

    return 1;
}

static int daE_GI_Delete(daE_GI_c* a_this) {
    return a_this->_delete();
}

int daE_GI_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_GI", 0x13);
    JUT_ASSERT(1621, modelData != NULL);

    mpModelMorf = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("E_GI", 0xB), 0, 1.0f, 0, -1, &mSound, 0x80000, 0x11000084);
    if (mpModelMorf == NULL || mpModelMorf->getModel() == NULL) {
        return 0;
    }

    J3DModel* model = mpModelMorf->getModel();
    model->setUserArea((uintptr_t)this);

    for (u16 i = 1; i < model->getModelData()->getJointNum(); i++) {
        if (i == 3) {
            model->getModelData()->getJointNodePointer(i)->setCallBack(JointCallBack);
        }
    }

    modelData = (J3DModelData*)dComIfG_getObjectRes("E_GI", 0x14);
    JUT_ASSERT(1652, modelData != NULL);
    mpSwordModel = mDoExt_J3DModel__create(modelData, 0, 0x11000084);
    if (mpSwordModel == NULL) {
        return 0;
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    return ((daE_GI_c*)i_this)->CreateHeap();
}

int daE_GI_c::create() {
    fopAcM_ct(this, daE_GI_c);

    OS_REPORT("E_GI PARAM %x\n", fopAcM_GetParam(this));
    mSwbit = fopAcM_GetParam(this);

    if (mSwbit != 0xFF && dComIfGs_isSwitch(mSwbit, fopAcM_GetRoomNo(this))) {
        // "After E_GI defeated, so not re-setting\n"
        OS_REPORT("E_GI やられ後なので再セットしません\n");
        return cPhs_ERROR_e;
    }

    mIsCreateAwake = (fopAcM_GetParam(this) >> 8);
    if (mIsCreateAwake) {
        mIsCreateAwake = TRUE;
    }

    mSwbit2 = (fopAcM_GetParam(this) >> 0x10);

    int phase_state = dComIfG_resLoad(&mPhase, "E_GI");
    if (phase_state == cPhs_COMPLEATE_e) {
        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x30A0)) {
            return cPhs_ERROR_e;
        }

        if (!hio_set) {
            hio_set = TRUE;
            mHIOInit = TRUE;
            l_HIO.id = mDoHIO_CREATE_CHILD("ギブド", &l_HIO);
        }

        attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        fopAcM_SetMtx(this, mpModelMorf->getModel()->getBaseTRMtx());
        fopAcM_SetMin(this, -200.0f, -200.0f, -200.0f);
        fopAcM_SetMax(this, 200.0f, 200.0f, 200.0f);

        mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
        mAcchCir.SetWall(20.0f, 100.0f);

        mWallCheckRadius = 150.0f;
        health = 240;
        field_0x560 = 240;

        mCcStts.Init(0xFE, 0, this);
    
        mCcSph[0].Set(cc_gi_src);
        mCcSph[0].SetStts(&mCcStts);

        mCcSph[1].Set(cc_gi_src);
        mCcSph[1].SetStts(&mCcStts);

        mAtSph[0].Set(cc_gi_att_src);
        mAtSph[0].SetStts(&mCcStts);

        mAtSph[1].Set(cc_gi_att_src);
        mAtSph[1].SetStts(&mCcStts);

        mAtSph[2].Set(cc_gi_att_src);
        mAtSph[2].SetStts(&mCcStts);

        mAtSph[3].Set(cc_gi_att_src);
        mAtSph[3].SetStts(&mCcStts);
    
        mSound.init(&current.pos, &eyePos, 3, 1);
        mSound.setEnemyName("E_gi");

        mAtInfo.mpSound = &mSound;
        mAtInfo.mPowerType = 1;
        gravity = -5.0f;
    
        if (!mIsCreateAwake) {
            setActionMode(ACTION_SLEEP_e, 0);
        } else {
            setActionMode(ACTION_WAIT_e, 0);
        }
    
        daE_GI_Execute(this);
    }

    return phase_state;
}

static int daE_GI_Create(daE_GI_c* a_this) {
    return a_this->create();
}

static DUSK_CONST actor_method_class l_daE_GI_Method = {
    (process_method_func)daE_GI_Create,
    (process_method_func)daE_GI_Delete,
    (process_method_func)daE_GI_Execute,
    (process_method_func)daE_GI_IsDelete,
    (process_method_func)daE_GI_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_GI = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_GI_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daE_GI_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_GI_e,
    /* Actor SubMtd */ &l_daE_GI_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_CULL_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
