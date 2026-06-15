/**
 * @file d_a_e_nz.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_e_nz.h"
#include "c/c_damagereaction.h"
#include "d/d_cc_d.h"
#include "Z2AudioLib/Z2Instances.h"
#include "f_op/f_op_actor_enemy.h"

#if TARGET_PC
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#include "dusk/coop/enemy_targeting.h"
#include "dusk/coop/ghost_rat_state_probe.h"
#include "dusk/coop/midna_owner.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/retained_interaction_owner.h"
#include "dusk/coop/selected_target_state.h"
#include "dusk/coop/world_switch_probe.h"
#endif

class daE_NZ_HIO_c : public JORReflexible {
public:
    daE_NZ_HIO_c();
    virtual ~daE_NZ_HIO_c() {}

    void genMessage(JORMContext*);

    /* 0x04 */ s8 mId;
    /* 0x08 */ f32 mBasicSize;
    /* 0x0C */ f32 mSpeed;
    /* 0x10 */ f32 mAttackSpeed;
    /* 0x14 */ s16 mWaitTime;
    /* 0x18 */ f32 mCurrentAlphaSpeed;
    /* 0x1C */ f32 mVanishingAlphaSpeed;
};

enum Action {
    ACTION_NORMAL,
    ACTION_ATTACK,
    ACTION_UNKNOWN,
    ACTION_STICK,
    ACTION_DAMANGE,
};

static u8 stick_bit[8] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
};

struct StickDef {
    s8 field_0x0;
    s16 field_0x2;
    s16 field_0x4;
    s16 field_0x6;
    s16 field_0x8;
};

static StickDef stick_d[8] = {
    0x03, 1500, 15000, 0, 0x14,
    0x00, 0, 32767,0, 0,
    0x01, 7000, -30000, 0x00, 0,
    0x02, 1000, 6000, 1500, 0x19,
    0x03, -25000, -10000, 22000, 0x14,
    0x02, 8000, -22000, -3000, 0x0F,
     0x01, 30000, -4100, 1500, 0x19,
     0x02,  5500, -20000, 24000, 0x0F,
};

daE_NZ_HIO_c::daE_NZ_HIO_c() {
    mId = -1;
    mBasicSize = 1.2f;
    mSpeed = 25.0f;
    mAttackSpeed = 45.0f;
    mWaitTime = 5;
    mCurrentAlphaSpeed = 60.0f;
    mVanishingAlphaSpeed = 30.0f;
}

#if DEBUG
void daE_NZ_HIO_c::genMessage(JORMContext* ctx) {
    // Ghost Rat
    ctx->genLabel("  幽霊ネズミ", 0x80000001);
    // Basic Size
    ctx->genSlider("基本サイズ", &mBasicSize, 0.0f, 5.0f);
    // Basic Speed
    ctx->genSlider("移動速度", &mSpeed, 0.0f, 30.0f);
    // Attack Speed
    ctx->genSlider("攻撃速度", &mAttackSpeed, 0.0f, 60.0f);
    // Attack Wait Time
    ctx->genSlider("出現タイムラグ", &mWaitTime, 0, 0x23);
    // Current alpha speed
    ctx->genSlider("現α速度", &mCurrentAlphaSpeed, 0.0f, 255.0f);
    // Vanishing alpha speed
    ctx->genSlider("消α速度", &mVanishingAlphaSpeed, 0.0f, 255.0f);
}
#endif

static void anm_init(e_nz_class* i_this, int param_2, f32 param_3, u8 param_4, f32 param_5) {
    i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_NZ", param_2), param_4,
                             param_3, param_5, 0.0f, -1.0f);
    i_this->field_0x5e4 = param_2;
}

static BOOL pl_check(e_nz_class* i_this, f32 param_1) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    dComIfGp_getPlayer(0);
    if (i_this->mPlayerDistance < param_1) {
        s16 angleDiff = a_this->shape_angle.y - i_this->mPlayerAngleY;
        if (angleDiff < 0x5000 && angleDiff > -0x5000) {
            return TRUE;
        }
    }
    return FALSE;
}

static int daE_NZ_Draw(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    J3DModel* model = i_this->mpMorf->getModel();
    if (i_this->field_0x5b8 != 0) {
        return 1;
    }

    g_env_light.settingTevStruct(0, &a_this->current.pos, &a_this->tevStr);
    if (i_this->mMaterialAlpha < 1.0f) {
        return 1;
    }

    g_env_light.setLightTevColorType_MAJI(model, &a_this->tevStr);
    J3DModelData* modelData = model->getModelData();
    u8 alpha = i_this->mMaterialAlpha;
    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        J3DGXColorS10* tevColor = modelData->getMaterialNodePointer(i)->getTevColor(2);
        tevColor->a = alpha;
    }
    i_this->mInvModel.entryDL(NULL);

    return 1;
}

static bool hio_set;

static daE_NZ_HIO_c l_HIO;

static u8 data_8072C454[4];

#if TARGET_PC
static dusk::coop::selected_target_state::SelectedTargetState s_CoOpTargetState;
static dusk::coop::ghost_rat_state_probe::GhostRatStateProbe s_CoOpWakeProbe;

// Co-op: Ghost Rat's native states consume cached player distance/angle fields; fill them from
// the Combat owner once per tick so attack and stick setup agree on the same selected player.
static bool coOpSelectGhostRatTarget(e_nz_class* i_this, const char* label,
                                     dusk::coop::EnemyTargetMode mode, bool committed) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;

    dusk::coop::EnemyTargetContext context;
    context.observer = a_this;
    context.scope = dusk::coop::EnemyTargetScope::Combat;
    context.mode = mode;
    context.label = label;
    context.committed = committed;

    const dusk::coop::EnemyTargetResult target = dusk::coop::selectEnemyTarget(context);
    s_CoOpTargetState = dusk::coop::selected_target_state::stateForEnemyTarget(target);
    dusk::coop::selected_target_state::recordSelectedTargetState(
        a_this, label, s_CoOpTargetState,
        s_CoOpTargetState.available
            ? dusk::coop::selected_target_state::SelectedTargetStateReason::EnemyTarget
            : dusk::coop::selected_target_state::SelectedTargetStateReason::InvalidTarget);

    if (!s_CoOpTargetState.available) {
        return false;
    }

    i_this->mPlayerDistance = target.distance;
    i_this->mPlayerAngleY = target.angleY;
    return true;
}

// Co-op: Ghost Rat's ceiling wake/drop gate is a vanilla awareness cache, not a retained combat
// target. Select the nearest active player in XZ so P2 underneath a ceiling rat can fill the same
// cached distance/angle fields that vanilla fills from P1, then let pl_check() keep the original
// full-distance and cone thresholds.
static bool coOpSelectGhostRatWakeTarget(e_nz_class* i_this, const char* label, f32 range) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    s_CoOpWakeProbe = {};
    s_CoOpWakeProbe.actor = reinterpret_cast<uintptr_t>(a_this);
    s_CoOpWakeProbe.actorId = fopAcM_GetID(a_this);
    s_CoOpWakeProbe.action = i_this->mAction;
    s_CoOpWakeProbe.subAction = i_this->mSubAction;
    s_CoOpWakeProbe.bck = i_this->field_0x5e4;
    s_CoOpWakeProbe.animFrame = i_this->mpMorf != NULL ? i_this->mpMorf->getFrame() : 0.0f;
    s_CoOpWakeProbe.checkRange = range;
    s_CoOpWakeProbe.label = label;

    dusk::coop::PlayerSlot bestSlot = dusk::coop::PlayerSlot::Invalid;
    fopAc_ac_c* bestActor = NULL;
    f32 bestDistance = 0.0f;
    f32 bestDistanceXZ = 0.0f;
    s16 bestAngleY = 0;
    bool found = false;

    dusk::coop::forEachActivePlayer([&](dusk::coop::PlayerSlot slot, fopAc_ac_c* actor) {
        const f32 distanceXZ = fopAcM_searchActorDistanceXZ(a_this, actor);
        if (distanceXZ >= range) {
            return;
        }

        if (!found || distanceXZ < bestDistanceXZ) {
            found = true;
            bestSlot = slot;
            bestActor = actor;
            bestDistance = fopAcM_searchActorDistance(a_this, actor);
            bestDistanceXZ = distanceXZ;
            bestAngleY = fopAcM_searchActorAngleY(a_this, actor);
        }
    });

    if (!found) {
        s_CoOpTargetState = dusk::coop::selected_target_state::SelectedTargetState{};
        dusk::coop::selected_target_state::recordSelectedTargetState(
            a_this, label, s_CoOpTargetState,
            dusk::coop::selected_target_state::SelectedTargetStateReason::NoMatch);
        return false;
    }

    const s16 angleDiff = a_this->shape_angle.y - bestAngleY;
    s_CoOpWakeProbe.wakeSlot = bestSlot;
    s_CoOpWakeProbe.wakeFound = true;
    s_CoOpWakeProbe.wakeDistance = bestDistance;
    s_CoOpWakeProbe.wakeDistanceXZ = bestDistanceXZ;
    s_CoOpWakeProbe.wakeAngleY = bestAngleY;
    s_CoOpWakeProbe.angleDiff = angleDiff;
    s_CoOpWakeProbe.rangeGate = bestDistance < range;
    s_CoOpWakeProbe.coneGate = angleDiff < 0x5000 && angleDiff > -0x5000;

    s_CoOpTargetState = dusk::coop::selected_target_state::stateForSlot(bestSlot, bestActor);
    dusk::coop::selected_target_state::recordSelectedTargetState(
        a_this, label, s_CoOpTargetState,
        dusk::coop::selected_target_state::SelectedTargetStateReason::FilteredNearest);
    i_this->mPlayerDistance = bestDistance;
    i_this->mPlayerAngleY = bestAngleY;
    (void)bestDistanceXZ;
    return true;
}

static bool coOpGhostRatHasFreeBodySlot() {
    if (data_8072C454[0] == 0xff) {
        return false;
    }

    for (int i = 0; i < 8; i++) {
        if ((data_8072C454[0] & stick_bit[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void coOpRecordGhostRatWakeProbe(e_nz_class* i_this, bool bodySlotAvailable,
                                        bool attackFrameGate, bool attackStarted) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    s_CoOpWakeProbe.actor = reinterpret_cast<uintptr_t>(a_this);
    s_CoOpWakeProbe.actorId = fopAcM_GetID(a_this);
    s_CoOpWakeProbe.action = i_this->mAction;
    s_CoOpWakeProbe.subAction = i_this->mSubAction;
    s_CoOpWakeProbe.bck = i_this->field_0x5e4;
    s_CoOpWakeProbe.animFrame = i_this->mpMorf != NULL ? i_this->mpMorf->getFrame() : 0.0f;
    s_CoOpWakeProbe.bodySlotAvailable = bodySlotAvailable;
    s_CoOpWakeProbe.attackFrameGate = attackFrameGate;
    s_CoOpWakeProbe.attackStarted = attackStarted;
    s_CoOpWakeProbe.reachedAction = true;
    // Co-op: diagnostics only. This records why the native Ghost Rat wake/drop branch did or did
    // not transition after the active-player cache was filled.
    dusk::coop::ghost_rat_state_probe::recordGhostRatStateProbe(s_CoOpWakeProbe);
}

static void coOpRecordGhostRatSwitchProbe(e_nz_class* i_this, bool switchOn) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    dusk::coop::ghost_rat_state_probe::GhostRatStateProbe probe;
    probe.actor = reinterpret_cast<uintptr_t>(a_this);
    probe.actorId = fopAcM_GetID(a_this);
    probe.action = i_this->mAction;
    probe.subAction = i_this->mSubAction;
    probe.bck = i_this->field_0x5e4;
    probe.animFrame = i_this->mpMorf != NULL ? i_this->mpMorf->getFrame() : 0.0f;
    probe.switchNo = i_this->field_0x5b8;
    probe.switchGateActive = i_this->field_0x5b8 != 0;
    probe.switchOn = switchOn;
    probe.label = "e_nz.switch_gate";
    // Co-op: diagnostics only. Some placed Ghost Rats are gated before the wake branch, so record
    // the native switch status separately from the player-distance wake probe.
    dusk::coop::ghost_rat_state_probe::recordGhostRatStateProbe(probe);
}

static bool coOpTryGhostRatSwitchGateWake(e_nz_class* i_this, int switchNo, int roomNo) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;

    // Co-op: some ceiling Ghost Rats are blocked before action() by an authored room switch. If
    // that gate is still closed, let the same active-player wake predicate open the same switch
    // instead of bypassing the gate; this preserves the native group switch/state
    // machine flow while allowing P2 to be the player who satisfies the authored wake facts.
    const bool selectedTarget = coOpSelectGhostRatWakeTarget(i_this, "e_nz.switch_wake", 700.0f);
    const bool canWake =
        selectedTarget && data_8072C454[0] != 0xff && pl_check(i_this, 700.0f);

    dusk::coop::ghost_rat_state_probe::GhostRatStateProbe probe = s_CoOpWakeProbe;
    probe.switchNo = switchNo;
    probe.switchGateActive = true;
    probe.switchOn = canWake;
    probe.reachedAction = false;
    probe.bodySlotAvailable = coOpGhostRatHasFreeBodySlot();
    probe.label = "e_nz.switch_wake";
    dusk::coop::ghost_rat_state_probe::recordGhostRatStateProbe(probe);

    if (!canWake) {
        return false;
    }

    const bool wasOnBefore = dComIfGs_isSwitch(switchNo, roomNo) != 0;
    dusk::coop::world_switch_probe::recordSwitchOn(a_this, switchNo, roomNo, wasOnBefore,
                                                   "e_nz.switch_wake");
    dusk::coop::world_switch_probe::suppressNextDirectSwitchOn(switchNo, roomNo);
    dComIfGs_onSwitch(switchNo, roomNo);
    return true;
}

static void coOpReleaseGhostRatAttach(e_nz_class* i_this, const char* label) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    // Co-op: native stick bits only track occupied body slots; clear the retained player owner
    // alongside the vanilla bit so detached rats stop affecting that slot.
    dusk::coop::retained_interaction_owner::clearRetainedInteraction(
        label, a_this, dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach);
}

// Co-op: Ghost Rat visibility is a world/sense question, so any active wolf-sense player should
// reveal it instead of requiring P1's wolf-sense state.
static bool coOpAnyPlayerWolfSenseActive(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    const dusk::coop::selected_target_state::SelectedTargetState senseState =
        dusk::coop::selected_target_state::findNearestPlayerState(
            a_this, "e_nz.wolf_sense",
            [](const dusk::coop::selected_target_state::SelectedTargetState& state) {
                return state.wolfSenseActive;
            });
    return senseState.available;
}
#endif

static void e_nz_normal(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    f32 dVar9 = 0.0f;
    cXyz local_44;
    if ((i_this->field_0x698 & 0x1f) == 0 && cM_rndF(1.0f) < 0.5f) {
        i_this->mSound.startCreatureVoice(Z2SE_EN_NZ_V_NAKU, -1);
    }

    switch(i_this->mSubAction) {
    case 0:
        anm_init(i_this, 9, 3.0f, 2, 1.0f);
        i_this->field_0x6a2[0] = cM_rndF(60.0f) + 30.0f;
        i_this->mSubAction = 1;
        i_this->field_0x698 = cM_rndF(65535.0f);
        break;
    case 1:
        if (i_this->mpMorf->getFrame() >= 1.0f && i_this->mpMorf->getFrame() <= 7.0f) {
            dVar9 = l_HIO.mSpeed;
            a_this->speedF = dVar9;
        }

        if (i_this->mpMorf->checkFrame(1.0f)) {
            i_this->field_0x5d4 += (int)cM_rndFX(3000.0f);
            a_this->speed.y = 20.0f;
        }

        if (i_this->mpMorf->checkFrame(9.0f)) {
            i_this->field_0xa78 = 1;
        }

        if (i_this->field_0x6a2[0] == 0 && i_this->mpMorf->checkFrame(10.0f)) {
            i_this->field_0x6a2[0] = cM_rndF(60.0f) + 30.0f;
            i_this->mSubAction = 2;
            if (cM_rndF(1.0f) < 0.5f) {
                anm_init(i_this, 10, 3.0f, 2, 1.0f);
            } else {
                anm_init(i_this, 8, 3.0f, 2, 1.0f);
            }
        }

        if (i_this->field_0x6a2[2] == 0 && fopAcM_wayBgCheck(a_this, 200.0f, 50.0f)) {
            i_this->field_0x6a2[2] = 20;
            i_this->field_0x6a2[1] = cM_rndF(10.0f) + 20.0f;
            i_this->field_0x5d4 = a_this->current.angle.y + 0x8000;
        }
        break;
    case 2:
        if (i_this->field_0x6a2[0] == 0) {
            i_this->mSubAction = 3;
            i_this->field_0x6a2[0] = 5;
            local_44.x = a_this->home.pos.x + cM_rndFX(500.0f) - a_this->current.pos.x;
            local_44.z = a_this->home.pos.z + cM_rndFX(500.0f) - a_this->current.pos.z;
            i_this->field_0x5d4 = cM_atan2s(local_44.x, local_44.z);
        }
        break;
    case 3:
        if (i_this->field_0x6a2[0] == 0) {
            i_this->mSubAction = 0;
        }
        break;
    }

    const bool canWake = data_8072C454[0] != 0xff && pl_check(i_this, 700.0f);
#if TARGET_PC
    const bool bodySlotAvailable = coOpGhostRatHasFreeBodySlot();
    const bool attackFrameGate =
        i_this->mPlayerDistance < 400.0f &&
        (i_this->field_0x5e4 == 8 ||
         ((i_this->field_0x5e4 == 9 && i_this->mpMorf->checkFrame(10.0f))));
    bool attackStarted = false;
#endif
    if (canWake) {
        i_this->field_0x5d4 = i_this->mPlayerAngleY;
        if (i_this->mPlayerDistance < 400.0f &&
            (i_this->field_0x5e4 == 8 ||
             ((i_this->field_0x5e4 == 9 && i_this->mpMorf->checkFrame(10.0f)))))
        {
            for (int i = 0; i < 8; i++) {
                if ((data_8072C454[0] & stick_bit[i]) == 0) {
                    data_8072C454[0] |= stick_bit[i];
                    i_this->field_0x6ac = i + 1;
#if TARGET_PC
                    // Co-op: once a Ghost Rat reserves a body slot, retain that player through
                    // the attack/stick lifetime instead of recomputing nearest player later.
                    dusk::coop::retained_interaction_owner::beginRetainedInteraction(
                        "e_nz.stick", a_this,
                        dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach,
                        s_CoOpTargetState.actor,
                        dusk::coop::retained_interaction_owner::RetainedInteractionReason::EnemyTarget);
#endif
                    break;
                }
            }
            i_this->mAction = ACTION_ATTACK;
            i_this->mSubAction = 0;
#if TARGET_PC
            attackStarted = true;
#endif
        }
    }
#if TARGET_PC
    coOpRecordGhostRatWakeProbe(i_this, bodySlotAvailable, attackFrameGate, attackStarted);
#endif
    cLib_addCalcAngleS2(&a_this->current.angle.y, i_this->field_0x5d4, 2, 0x2000);
    cLib_addCalc2(&a_this->speedF, dVar9, 1.0f, l_HIO.mSpeed * 0.25f);
}

static s8 e_nz_attack(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    cXyz local_38;
    s8 rv = 0;
    switch(i_this->mSubAction) {
    case 0:
        anm_init(i_this, 7, 3.0f, 0, 1.0f);
        i_this->mSubAction = 1;
        a_this->speedF = 0.0f;
        break;
    case 1:
        i_this->field_0x5c8 = i_this->field_0x5bc;
        local_38 = i_this->field_0x5bc - a_this->current.pos;
        a_this->current.angle.y = cM_atan2s(local_38.x, local_38.z);
        a_this->current.angle.x = -cM_atan2s(local_38.y, JMAFastSqrt(local_38.x * local_38.x + local_38.z * local_38.z));
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        anm_init(i_this, 6, 5.0f, 0, 1.0f);
        i_this->mSubAction = 2;
        a_this->speedF = l_HIO.mAttackSpeed;
        i_this->field_0x5dc = 25.0f;
        i_this->field_0x6a2[0] = 20;
        // fallthrough
    case 2:
        rv = 1;
        local_38 = i_this->field_0x5bc - a_this->current.pos;
        if (local_38.abs() < a_this->speedF * 2.0f) {
            i_this->mSubAction = 3;
        } else {
            local_38 = i_this->field_0x5c8 - i_this->field_0x5bc;
            if (local_38.abs() > 50.0f || i_this->field_0x6a2[0] == 0) {
                i_this->mSubAction = 5;
            }
        }
        break;
    case 3:
        i_this->field_0x5c8 = i_this->field_0x5bc;
        rv = 2;
        a_this->speedF = 0.0f;
        cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x5bc.x, 1.0f, 100.0f);
        cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x5bc.y, 1.0f, 100.0f);
        cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x5bc.z, 1.0f, 100.0f);
        local_38 = i_this->field_0x5bc - a_this->current.pos;
        if (local_38.abs() < 5.0f) {
            i_this->mAction = ACTION_STICK;
            i_this->mSubAction = 0;
        }
        break;
    case 5:
        cLib_addCalcAngleS2(&a_this->current.angle.x, 0, 1, 0x1000);
        if (i_this->mAcch.ChkGroundHit() || i_this->field_0x8c8) {
            i_this->mAction = ACTION_NORMAL;
            i_this->field_0x6a2[0] =  cM_rndF(30.0f) + 30.0f;
            i_this->mSubAction = 2;
            anm_init(i_this, 8, 3.0f, 2, 1.0f);
            a_this->current.angle.x = 0;
#if TARGET_PC
            coOpReleaseGhostRatAttach(i_this, "e_nz.attack_miss");
#endif
            data_8072C454[0] &= ~stick_bit[i_this->field_0x6ac - 1];
            i_this->field_0x6ac = 0;
        }
        break;
    }
    
    return rv;
}

static void e_nz_stick(e_nz_class* i_this) {
#if !TARGET_PC
    s8 cVar4 = 0;
#endif
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;

    switch(i_this->mSubAction) {
    case 0:
        anm_init(i_this, 5, 3.0f, 2, 1.0f);
        i_this->mSubAction = 1;
        dComIfGp_getVibration().StartShock(1, 1, cXyz(0.0f, 1.0f, 0.0f));
        break;
    case 1:
        if (i_this->mpMorf->checkFrame(2.0f)) {
            i_this->mSound.startCreatureSound(Z2SE_EN_NZ_BITE, 0, -1);
        }
#if TARGET_PC
        {
            // Co-op: heavy-state is caused by rats retained on a specific player's body, not by
            // the global P1 rat counter.
            dusk::coop::retained_interaction_owner::RetainedInteractionState owner =
                dusk::coop::retained_interaction_owner::updateRetainedInteraction(
                    "e_nz.stick", a_this,
                    dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach);
            if (owner.found &&
                dusk::coop::retained_interaction_owner::countRetainedInteractions(
                    owner.slot,
                    dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach) >= 3)
            {
                owner.localPlayer->onHeavyState();
            }
        }
#else
        for (int i = 0; i < 8; i++) {
            if ((data_8072C454[0] & stick_bit[i]) != 0) {
                cVar4++;
            }
        }
        if (cVar4 >= 3) {
            daPy_getLinkPlayerActorClass()->onHeavyState();
        }
#endif
        break;
    }
}

static void damage_check(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    dComIfGp_getPlayer(0);
    if (i_this->field_0x6aa == 0) {
        i_this->mStts.Move();
        if (i_this->mSph.ChkTgHit()) {
            i_this->mAtInfo.mpCollider = i_this->mSph.GetTgHitObj();
            cc_at_check(a_this, &i_this->mAtInfo);
            if (i_this->mAtInfo.mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK | AT_TYPE_10000000 | AT_TYPE_WOLF_CUT_TURN | AT_TYPE_WOLF_ATTACK)) {
                i_this->field_0x6aa = 20;
            } else {
                i_this->field_0x6aa = 10;
            }

            if (i_this->mAction == ACTION_STICK) {
                a_this->current.angle.y = (i_this->field_0x6ac - 1) * 0x2000;
            } else {
                a_this->current.angle.y = i_this->mAtInfo.mHitDirection.y;
            }
            i_this->mAction = ACTION_DAMANGE;
            i_this->mSubAction = 0;
            i_this->mSound.startCreatureVoice(Z2SE_EN_NZ_V_DEATH, -1);
        }

        if (a_this->health <= 1) {
            a_this->health = 0;
            i_this->mSph.SetTgHitMark(CcG_Tg_UNK_MARK_3);
        }
    }
}

static void e_nz_damage(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    i_this->field_0x6aa = 6;
    switch (i_this->mSubAction) {
    case 0:
        anm_init(i_this, 4, 5.0f, 0, 1.0f);
        i_this->mSubAction = 1;
        a_this->speed.y = cM_rndF(10.0f) + 40.0f;
        a_this->speedF = -30.0f;
        i_this->field_0x6a2[0] = cM_rndF(5.0f) + 15.0f;
        // fallthrough
    case 1:
        if (i_this->field_0x6a2[0] == 0) {
            fopAcM_createDisappear(a_this, &a_this->eyePos, 6, 3, 0x27);
            if (i_this->field_0x5b6 == 1) {
                a_this->current = a_this->home;
                a_this->old = a_this->current;
                a_this->health = 10;
                i_this->mAction = ACTION_NORMAL;
                i_this->mSubAction = 0;
                i_this->mMaterialAlpha = 0.0f;
                if (i_this->field_0x6ac != 0) {
#if TARGET_PC
                    coOpReleaseGhostRatAttach(i_this, "e_nz.damage_reset");
#endif
                    data_8072C454[0] &= ~stick_bit[i_this->field_0x6ac - 1];
                    i_this->field_0x6ac = 0;
                }
            } else {
                fopAcM_delete(a_this);
            }
        }
        break;
    }
}

static BOOL getPolyColor(cBgS_PolyInfo& param_1, int param_2, GXColor* param_3,
                             GXColor* param_4, u8* param_5, f32* param_6) {
    if (!dComIfG_Bgsp().ChkPolySafe(param_1)) {
        return FALSE;
    }

    if (param_2 == 0) {
        dKy_pol_eff_prim_get(&param_1, param_3);
        dKy_pol_eff_env_get(&param_1, param_4);
        *param_5 = dKy_pol_eff_alpha_get(&param_1);
        *param_6 = dKy_pol_eff_ratio_get(&param_1);
    } else {
        dKy_pol_eff2_prim_get(&param_1, param_3);
        dKy_pol_eff2_env_get(&param_1, param_4);
        *param_5 = dKy_pol_eff2_alpha_get(&param_1);
        *param_6 = dKy_pol_eff2_ratio_get(&param_1);
    }

    return TRUE;
}

static s8 action(e_nz_class* i_this) {
    static u16 eff_id[4] = {
        0x01B8,
        0x01B9,
        0x01BA,
        0x01BB,
    };

    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    cXyz local_74;
    cXyz local_80;
#if TARGET_PC
    bool selectedTarget = false;
    if (i_this->mAction == ACTION_NORMAL) {
        selectedTarget = coOpSelectGhostRatWakeTarget(i_this, "e_nz.wake", 700.0f);
    } else {
        // Co-op: keep Ghost Rat's native player-distance/angle cache, but source it from the
        // selected Combat target so P2 can be attacked and attached consistently after wake-up.
        const bool committed = i_this->mAction == ACTION_ATTACK || i_this->mAction == ACTION_STICK;
        selectedTarget = coOpSelectGhostRatTarget(i_this, "e_nz.action",
                                                  dusk::coop::EnemyTargetMode::StickyCombat,
                                                  committed);
    }
    if (!selectedTarget) {
        i_this->mPlayerDistance = fopAcM_searchPlayerDistance(a_this);
        i_this->mPlayerAngleY = fopAcM_searchPlayerAngleY(a_this);
    }
#else
    i_this->mPlayerDistance = fopAcM_searchPlayerDistance(a_this);
    i_this->mPlayerAngleY = fopAcM_searchPlayerAngleY(a_this);
#endif
    damage_check(i_this);
    s8 action_result = 0;
    s8 is_active = 0;

    switch (i_this->mAction) {
    case ACTION_NORMAL:
        e_nz_normal(i_this);
        break;
    case ACTION_DAMANGE:
        e_nz_damage(i_this);
        break;
    case ACTION_ATTACK:
        action_result = e_nz_attack(i_this);
        is_active = 1;
        break;

    case ACTION_STICK:
#if TARGET_PC
        {
            // Co-op: Midna's rat-body panic belongs to the player the rat is retained on.
            dusk::coop::retained_interaction_owner::RetainedInteractionState owner =
                dusk::coop::retained_interaction_owner::updateRetainedInteraction(
                    "e_nz.stick", a_this,
                    dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach);
            if (owner.found) {
                if (daMidna_c* midna =
                        dusk::coop::midna_owner::getMidnaForPlayer(
                            static_cast<daAlink_c*>(owner.localPlayerActor)))
                {
                    midna->onRatBody(0);
                }
            }
        }
#else
        if (daPy_py_c::getMidnaActor()) {
            daPy_py_c::getMidnaActor()->onRatBody(0);
        }
#endif
        e_nz_stick(i_this);
        action_result = 3;
        is_active = 1;
        break;
    }

    if (is_active) {
        i_this->mSound.setLinkSearch(true);
    } else {
        i_this->mSound.setLinkSearch(false);
    }

    cLib_addCalcAngleS2(&a_this->shape_angle.y, a_this->current.angle.y, 2, 0x2000);
    cLib_addCalcAngleS2(&a_this->shape_angle.x, a_this->current.angle.x, 2, 0x2000);
    cLib_addCalcAngleS2(&a_this->shape_angle.z, a_this->current.angle.z, 2, 0x2000);

    if (action_result == 0) {
        cMtx_YrotS(*calc_mtx, a_this->current.angle.y);
        local_74.x = 0.0f;
        local_74.y = 0.0f;
        local_74.z = a_this->speedF;
        MtxPosition(&local_74, &local_80);
        a_this->speed.x = local_80.x;
        a_this->speed.z = local_80.z;
        a_this->current.pos += a_this->speed * l_HIO.mBasicSize;
        a_this->speed.y += a_this->gravity;
        a_this->gravity = -8.0f;
        if (a_this->speed.y < -120.0f) {
            a_this->speed.y = -120.0f;
        }
    } else if (action_result == 1) {
        cMtx_YrotS(*calc_mtx, a_this->current.angle.y);
        cMtx_XrotM(*calc_mtx, a_this->current.angle.x);
        local_74.x = 0.0f;
        local_74.y = 0.0f;
        local_74.z = a_this->speedF;
        MtxPosition(&local_74, &a_this->speed);
        a_this->current.pos += a_this->speed * l_HIO.mBasicSize;
    }

    i_this->mAcch.CrrPos(dComIfG_Bgsp());

    dBgS_ObjGndChk_Spl gnd_chk;
    i_this->field_0x8c8 = 0;
    local_74 = a_this->current.pos;
    local_74.y += 200.0f;
    gnd_chk.SetPos(&local_74);
    if (a_this->current.pos.y <= dComIfG_Bgsp().GroundCross(&gnd_chk)) {
        a_this->current.pos.y = dComIfG_Bgsp().GroundCross(&gnd_chk);
        a_this->speed.y = 0.0f;
        i_this->field_0x8c8 = 1;
    }

    if (i_this->field_0xa78) {
        if (i_this->mPlayerDistance < 5000.0f) {
            static cXyz sc(0.35f, 0.35f, 0.35f);
            u8 alpha;
            f32 ratio;
            GXColor prim_color;
            GXColor env_color;
            if (getPolyColor(i_this->mAcch.m_gnd, 0, &prim_color, &env_color, &alpha, &ratio)) {
                if (i_this->field_0x8c8 != 0) {
                    for (int i = 0; i < 4; i++) {
                        i_this->mParticleIds[i] = dComIfGp_particle_setColor(
                            i_this->mParticleIds[i], eff_id[i], &a_this->current.pos,
                            &a_this->tevStr, &prim_color, &env_color, ratio, alpha,
                            &a_this->shape_angle, &sc, NULL, -1, NULL);
                    }
                    i_this->mSound.startCreatureSound(Z2SE_EN_NZ_FN_WATER, 0, -1);
                } else {
                    i_this->mParticle = dComIfGp_particle_setColor(
                        i_this->mParticle, 0xe6, &a_this->current.pos, &a_this->tevStr,
                        &prim_color, &env_color, ratio, alpha, &a_this->shape_angle, &sc, NULL, -1,
                        NULL);
                    JPABaseEmitter* pEmitter =
                        dComIfGp_particle_getEmitter(i_this->mParticle);
                    if (pEmitter != NULL) {
                        pEmitter->setRate(1.0f);
                        i_this->mSound.startCreatureSound(Z2SE_EN_NZ_FOOTNOTE, 0, -1);
                    }
                }
            }
        }
        i_this->field_0xa78 = 0;
    }

    cXyz bind_scale(0.5f, 0.5f, 0.5f);
    setMidnaBindEffect(a_this, &i_this->mSound, &a_this->eyePos, &bind_scale);

    return action_result;
}

static int daE_NZ_Execute(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    
    f32 alphaTarget = 0.0f;
    f32 alphaStep = l_HIO.mVanishingAlphaSpeed;
#if TARGET_PC
    if (coOpAnyPlayerWolfSenseActive(i_this)) {
#else
    if (daPy_py_c::checkNowWolfPowerUp()) {
#endif
        if (i_this->field_0x6a2[3] == 0) {
            alphaTarget = 255.0f;
            alphaStep = l_HIO.mCurrentAlphaSpeed;
        }
    } else {
        i_this->field_0x6a2[3] = l_HIO.mWaitTime;
    }

    cLib_addCalc2(&i_this->mMaterialAlpha, alphaTarget, 1.0f, alphaStep);
    
    if (cDmrNowMidnaTalk() || dComIfGp_event_runCheck()) {
        return 1;
    }

    cXyz local_58;
    cXyz local_64;
    cXyz local_70;
    
    if (i_this->field_0x5b8 != 0) {
        const int switchNo = i_this->field_0x5b8;
        const int roomNo = fopAcM_GetRoomNo(a_this);
        bool switchOn = dComIfGs_isSwitch(switchNo, roomNo);
#if TARGET_PC
        if (!switchOn) {
            switchOn = coOpTryGhostRatSwitchGateWake(i_this, switchNo, roomNo);
        } else {
            coOpRecordGhostRatSwitchProbe(i_this, switchOn);
        }
#else
        (void)switchNo;
#endif
        if (switchOn) {
            i_this->field_0x5b8 = 0;
        } else {
            return 1;
        }
    }
    
    i_this->field_0x698++;
    
    for (int i = 0; i < 4; i++) {
        if (i_this->field_0x6a2[i] != 0) {
            i_this->field_0x6a2[i]--;
        }
    }
    
    if (i_this->field_0x6aa != 0) {
        i_this->field_0x6aa--;
    }
    
    J3DModel* model = i_this->mpMorf->getModel();
    
    if (i_this->mAction == ACTION_STICK || i_this->mAction == ACTION_ATTACK) {
#if TARGET_PC
        // Co-op: attached rats follow the retained player's body joint, not P1's joint matrix.
        dusk::coop::retained_interaction_owner::RetainedInteractionState owner =
            dusk::coop::retained_interaction_owner::updateRetainedInteraction(
                "e_nz.attach_matrix", a_this,
                dusk::coop::retained_interaction_owner::RetainedInteractionScope::Attach);
        daPy_py_c* player =
            owner.found ? owner.localPlayer : daPy_getLinkPlayerActorClass();
#else
        daPy_py_c* player = daPy_getLinkPlayerActorClass();
#endif
        MtxP joint_mtx = player->getModelJointMtx(stick_d[i_this->field_0x6ac - 1].field_0x0);
        MTXCopy(joint_mtx, *calc_mtx);
        cMtx_YrotM(*calc_mtx, stick_d[i_this->field_0x6ac - 1].field_0x2);
        cMtx_XrotM(*calc_mtx, stick_d[i_this->field_0x6ac - 1].field_0x4);
        cMtx_ZrotM(*calc_mtx, stick_d[i_this->field_0x6ac - 1].field_0x6);
        MtxTrans(0.0f, stick_d[i_this->field_0x6ac - 1].field_0x8, 0.0f, 1);
        local_58.set(0.0f, 0.0f, 0.0f);
        MtxPosition(&local_58, &i_this->field_0x5bc);
        
        if (i_this->mAction == ACTION_STICK) {
            model->setBaseTRMtx(*calc_mtx);
            a_this->current.pos = i_this->field_0x5bc;
        }
    }
    
    if (action(i_this) != 3) {
        i_this->field_0x5d8 += i_this->field_0x5dc;
        i_this->field_0x5dc -= 8.0f;
        if (i_this->field_0x5d8 < 0.0f) {
            i_this->field_0x5dc = 0.0f;
            i_this->field_0x5d8 = 0.0f;
        }
        
        mDoMtx_stack_c::transS(a_this->current.pos.x,
                                a_this->current.pos.y + i_this->field_0x5d8,
                                a_this->current.pos.z);
        mDoMtx_stack_c::YrotM(a_this->shape_angle.y);
        mDoMtx_stack_c::XrotM(a_this->shape_angle.x);
        mDoMtx_stack_c::ZrotM(a_this->shape_angle.z);
        mDoMtx_stack_c::scaleM(l_HIO.mBasicSize, l_HIO.mBasicSize, l_HIO.mBasicSize);
        model->setBaseTRMtx(mDoMtx_stack_c::get());
    }
    
    i_this->mpMorf->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(a_this)));
    i_this->mpMorf->modelCalc();
    
    MtxP joint_mtx = model->getAnmMtx(6);
    cMtx_copy(joint_mtx, *calc_mtx);
    local_58.set(0.0f, 0.0f, 0.0f);
    MtxPosition(&local_58, &a_this->eyePos);
    a_this->attention_info.position = a_this->eyePos;
    a_this->attention_info.position.y += 40.0f;
    local_58.set(0.0f, 0.0f, 0.0f);
    MtxPosition(&local_58, &local_64);
    
    if (i_this->mMaterialAlpha < 1.0f) {
        local_64.z += 10000.0f;
        fopAcM_OffStatus(a_this, 0);
        a_this->attention_info.flags = 0;
    } else {
        if (i_this->mAction == ACTION_STICK) {
            i_this->mSph.SetTgType(0x40000002);
            if ((i_this->field_0x698 & 0x20) != 0) {
                dComIfGp_att_LookRequest(a_this, 400.0f,300.0f, -300.0f, 0x6000, 1);
            }
            fopAcM_OffStatus(a_this, 0);
            a_this->attention_info.flags = 0;
        } else {
            i_this->mSph.SetTgType(0xd8fbfdff);
            fopAcM_OnStatus(a_this, 0);
            a_this->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        }
    }
    
    i_this->mSph.SetC(local_64);
    i_this->mSph.SetR(25.0f * l_HIO.mBasicSize);
    dComIfG_Ccsp()->Set(&i_this->mSph);
    return 1;
}

static int daE_NZ_IsDelete(e_nz_class* i_this) {
    return 1;
}

static int daE_NZ_Delete(e_nz_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)&i_this->enemy;
    fopAcM_GetID(i_this);
    dComIfG_resDelete(&i_this->mPhase, "E_NZ");
    if (i_this->mIsHIOOwner) {
        hio_set = 0;
        mDoHIO_DELETE_CHILD(l_HIO.mId);
    }

    if (a_this->heap != NULL) {
        i_this->mpMorf->stopZelAnime();
    }

    if (i_this->field_0x6ac != 0) {
#if TARGET_PC
        coOpReleaseGhostRatAttach(i_this, "e_nz.delete");
#endif
        data_8072C454[0] &= ~stick_bit[i_this->field_0x6ac - 1];
        i_this->field_0x6ac = 0;
    }
#if TARGET_PC
    // Co-op: delete can run after non-attached states too, so clear any remaining retained owner.
    dusk::coop::retained_interaction_owner::clearAllRetainedInteractions(a_this);
    dusk::coop::ghost_rat_state_probe::clearGhostRatStateProbe(a_this);
#endif
    return 1;
}

static int useHeapInit(fopAc_ac_c* a_this) {
    e_nz_class* i_this = (e_nz_class*)a_this;
    i_this->mpMorf = JKR_NEW mDoExt_McaMorfSO((J3DModelData*)dComIfG_getObjectRes("E_NZ", 13), NULL,
                                          NULL, (J3DAnmTransform*)dComIfG_getObjectRes("E_NZ", 10),
                                          2, 1.0f, 0, -1, &i_this->mSound, 0x80000, 0x11000084);
    if (i_this->mpMorf == NULL || i_this->mpMorf->getModel() == NULL) {
        return 0;
    }

    MtxScale(0.0f, 0.0f, 0.0f, 0);
    i_this->mpMorf->getModel()->setBaseTRMtx(*calc_mtx);
    if (i_this->mInvModel.create(i_this->mpMorf->getModel(), 1) == 0) {
        return 0;
    }
    return 1;
}

static int daE_NZ_Create(fopAc_ac_c* a_this) {
    e_nz_class* i_this = (e_nz_class*)a_this;
    fopAcM_ct(a_this, e_nz_class);
    int phase = dComIfG_resLoad(&i_this->mPhase, "E_NZ");
    if (phase == cPhs_COMPLEATE_e) {
        OS_REPORT("E_NZ PARAM %x\n", fopAcM_GetParam(a_this));
        i_this->field_0x5b6 = fopAcM_GetParam(a_this) & 0xff;
        i_this->field_0x5b7 = (fopAcM_GetParam(a_this) & 0xff00) >> 8;
        i_this->field_0x5b8 = (fopAcM_GetParam(a_this) & 0xff000000) >> 24;
        OS_REPORT("E_NZ//////////////E_NZ SET 1 !!\n");

        if (fopAcM_entrySolidHeap(a_this, useHeapInit, 0x17e0) == 0) {
            OS_REPORT("//////////////E_NZ SET NON !!\n");
            return cPhs_ERROR_e;
        }

        if (!hio_set) {
            i_this->mIsHIOOwner = 1;
            hio_set = true;
            // Ghost Rat
            l_HIO.mId = mDoHIO_CREATE_CHILD("幽霊ネズミ", &l_HIO);
        }
        fopAcM_SetMtx(a_this, i_this->mpMorf->getModel()->getBaseTRMtx());
        a_this->health = 10;
        a_this->field_0x560 = 10;
        i_this->mStts.Init(100, 0, a_this);

        static dCcD_SrcSph cc_sph_src = {
            {
                {0x0, {{0x0, 0x0, 0x0}, {0xd8fbfdff, 0x3}, 0x0}},  // mObj
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2},                // mGObjTg
                {0x0},                                             // mGObjCo
            },                                                     // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 40.0f}  // mSph
            }  // mSphAttr
        };

        i_this->mSph.Set(cc_sph_src);
        i_this->mSph.SetStts(&i_this->mStts);
        i_this->mAcch.Set(fopAcM_GetPosition_p(a_this), fopAcM_GetOldPosition_p(a_this), a_this, 1,
                            &i_this->mAcchCir, fopAcM_GetSpeed_p(a_this), NULL, NULL);
        i_this->mAcchCir.SetWall(30.0f, 30.0f);
        i_this->mSound.init(&a_this->current.pos, &a_this->eyePos, 3, 1);
        i_this->mSound.setEnemyName("E_nz");
        i_this->mAtInfo.mpSound = &i_this->mSound;
        i_this->mAtInfo.mPowerType = 1;
        i_this->field_0x698 = cM_rndF(65535.0f);
        daE_NZ_Execute(i_this);
        g_env_light.settingTevStruct(0, &(a_this->current).pos, &a_this->tevStr);
    }
    return phase;
}

AUDIO_INSTANCES

static DUSK_CONST actor_method_class l_daE_NZ_Method = {
    (process_method_func)daE_NZ_Create,
    (process_method_func)daE_NZ_Delete,
    (process_method_func)daE_NZ_Execute,
    (process_method_func)daE_NZ_IsDelete,
    (process_method_func)daE_NZ_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_NZ = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_NZ_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(e_nz_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_NZ_e,
    /* Actor SubMtd */ &l_daE_NZ_Method,
    /* Status       */ fopAcStts_UNK_0x10000000_e | fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e | fopAcStts_CULL_e | fopAcStts_UNK_0x20_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
