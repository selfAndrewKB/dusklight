/**
* @file d_a_e_bug.cpp
 *
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_e_bug.h"
#include "d/actor/d_a_player.h"
#include "d/d_path.h"
#include "d/actor/d_a_nbomb.h"
#include "Z2AudioLib/Z2Instances.h"
#include <cstring>

#if TARGET_PC
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_e_nest.h"
#include "dusk/coop/enemy_targeting.h"
#include "dusk/coop/item_awareness.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/selected_target_state.h"
#endif

enum E_bug_RES_File_ID {
    /* BMDG */
    /* 0x3 */ BMDG_MU04 = 0x3,
    /* 0x4 */ BMDG_MU05,
};

daE_Bug_HIO_c::daE_Bug_HIO_c() {
    field_0x4 = -1;
    field_0x8 = 1.5f;
    field_0xc = 1.0f;
}

static u8 hio_set;

static daE_Bug_HIO_c l_HIO;

static s8 l_roomNo;

static s8 data_80697E8D;

#if TARGET_PC
namespace {

struct CoOpBugTargetState {
    dusk::coop::selected_target_state::SelectedTargetState state;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
};

struct CoOpBugExactPlayerData {
    fopAc_ac_c* player = NULL;
};

struct CoOpBugWakeCandidateData {
    e_bug_class* swarm = NULL;
};

static dusk::coop::PlayerQueryEligibility coOpBugExactPlayer(
    dusk::coop::PlayerSlot, fopAc_ac_c* actor, void* userData) {
    dusk::coop::PlayerQueryEligibility eligibility;
    CoOpBugExactPlayerData* data = static_cast<CoOpBugExactPlayerData*>(userData);
    if (data == NULL || actor != data->player) {
        eligibility.eligible = false;
        eligibility.failureFlags = dusk::coop::PlayerQueryEligibilityFailure_Status;
    }
    return eligibility;
}

static dusk::coop::PlayerQueryEligibility coOpBugWakeCandidate(
    dusk::coop::PlayerSlot, fopAc_ac_c* actor, void* userData) {
    dusk::coop::PlayerQueryEligibility eligibility;
    CoOpBugWakeCandidateData* data = static_cast<CoOpBugWakeCandidateData*>(userData);
    if (actor == NULL || data == NULL || data->swarm == NULL) {
        eligibility.eligible = false;
        eligibility.failureFlags = dusk::coop::PlayerQueryEligibilityFailure_Status;
        return eligibility;
    }

    bool inRange = false;
    for (int i = 0; i < data->swarm->bug_num; i++) {
        const bug_s& mite = data->swarm->Bug_s[i];
        if (mite.field_0x50 == -1 &&
            mite.field_0x18.abs(actor->current.pos) < data->swarm->field_0x57c)
        {
            inRange = true;
            break;
        }
    }
    if (!inRange) {
        eligibility.eligible = false;
        eligibility.failureFlags = dusk::coop::PlayerQueryEligibilityFailure_Range;
    }
    return eligibility;
}

// Co-op: one Poison Mite actor represents one swarm, so every internal mite shares one Combat
// owner. Attachment commits that owner so body-bound mites cannot jump to another player.
static bool coOpSelectBugTargetState(
    e_bug_class* i_this, const char* label, bool committed, dusk::coop::EnemyTargetMode mode,
    CoOpBugTargetState* out, dusk::coop::PlayerQueryPredicate predicate = NULL,
    void* predicateData = NULL) {
    fopAc_ac_c* actor = &i_this->actor;
    dusk::coop::EnemyTargetContext context;
    context.observer = actor;
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
        actor, label, targetState,
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

static bool coOpBugSelectExactPlayer(e_bug_class* i_this, fopAc_ac_c* player,
                                     const char* label, CoOpBugTargetState* out) {
    if (player == NULL) {
        return false;
    }
    CoOpBugExactPlayerData data;
    data.player = player;
    // Co-op: producer ownership from a nest hit or local wake must replace stale swarm retention.
    return coOpSelectBugTargetState(i_this, label, false,
                                    dusk::coop::EnemyTargetMode::ImmediateAcquire, out,
                                    coOpBugExactPlayer, &data);
}

static daPy_py_c* coOpBugTargetPlayer(const CoOpBugTargetState& target) {
    return target.state.available && target.state.player != NULL
               ? target.state.player
               : daPy_getPlayerActorClass();
}

static daPy_py_c* coOpBugRetainedTargetPlayer(e_bug_class* i_this) {
    const dusk::coop::EnemyTargetResult target =
        dusk::coop::getEnemyTarget(&i_this->actor, dusk::coop::EnemyTargetScope::Combat);
    const dusk::coop::selected_target_state::SelectedTargetState state =
        dusk::coop::selected_target_state::stateForEnemyTarget(target);
    return state.available && state.player != NULL ? state.player : daPy_getPlayerActorClass();
}

static fopAc_ac_c* coOpBugNestHitOwner(e_bug_class* i_this) {
    fopAc_ac_c* parent = fopAcM_SearchByID(i_this->actor.parentActorID);
    if (parent == NULL || fopAcM_GetName(parent) != fpcNm_E_NEST_e) {
        return NULL;
    }
    e_nest_class* nest = static_cast<e_nest_class*>(parent);
    if (nest->mHitActorID == fpcM_ERROR_PROCESS_ID_e) {
        return NULL;
    }
    fopAc_ac_c* hitOwner = fopAcM_SearchByID(nest->mHitActorID);
    return dusk::coop::getSlotForActor(hitOwner) != dusk::coop::PlayerSlot::Invalid ? hitOwner
                                                                                    : NULL;
}

static int coOpBugAttachedCount(e_bug_class* i_this) {
    int count = 0;
    for (int i = 0; i < i_this->bug_num; i++) {
        if (i_this->Bug_s[i].field_0x50 == 2) {
            count++;
        }
    }
    return count;
}

static bool coOpBugAllWaiting(e_bug_class* i_this) {
    for (int i = 0; i < i_this->bug_num; i++) {
        if (i_this->Bug_s[i].field_0x50 > 0 ||
            (i_this->Bug_s[i].field_0x50 == -1 && i_this->Bug_s[i].field_0x51 != 0))
        {
            return false;
        }
    }
    return true;
}

static void coOpPrepareBugTarget(e_bug_class* i_this, CoOpBugTargetState* target) {
    if (target == NULL) {
        return;
    }
    *target = {};

    const int attachedCount = coOpBugAttachedCount(i_this);
    const bool allWaiting = coOpBugAllWaiting(i_this);
    fopAc_ac_c* nestOwner = allWaiting ? coOpBugNestHitOwner(i_this) : NULL;
    if (nestOwner != NULL &&
        coOpBugSelectExactPlayer(i_this, nestOwner, "e_bug.nest_owner", target))
    {
        return;
    }

    if (allWaiting) {
        if (i_this->bitSw != 0xFF) {
            if (dComIfGs_isSwitch(i_this->bitSw, l_roomNo)) {
                coOpSelectBugTargetState(
                    i_this, "e_bug.switch_wake", false,
                    dusk::coop::EnemyTargetMode::ImmediateAcquire, target);
            }
            return;
        }

        CoOpBugWakeCandidateData wakeData;
        wakeData.swarm = i_this;
        // Co-op: choose one eligible swarm owner before individual sleeping mites update.
        coOpSelectBugTargetState(
            i_this, "e_bug.proximity_wake", false,
            dusk::coop::EnemyTargetMode::ImmediateAcquire, target,
            coOpBugWakeCandidate, &wakeData);
        return;
    }

    coOpSelectBugTargetState(i_this, "e_bug.swarm", attachedCount != 0,
                             dusk::coop::EnemyTargetMode::StickyCombat, target);
}

static bool coOpBugRightOrLeftTurnCut(int cutType) {
    return cutType == daPy_py_c::CUT_TYPE_TURN_RIGHT ||
           cutType == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT ||
           cutType == daPy_py_c::CUT_TYPE_TURN_LEFT ||
           cutType == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT;
}

static s8 coOpBugCheckPlayerDamage(bug_s* i_this, cXyz* hitDelta) {
    s8 hit = 0;
    dusk::coop::forEachActivePlayer([&](dusk::coop::PlayerSlot, fopAc_ac_c* actor) {
        if (hit != 0 || actor == NULL || fopAcM_GetName(actor) != fpcNm_ALINK_e) {
            return;
        }

        daPy_py_c* player = static_cast<daPy_py_c*>(actor);
        cXyz attackPos;
        f32 radius = 70.0f;
        if (player->checkWolf()) {
            attackPos = player->current.pos;
        } else if (coOpBugRightOrLeftTurnCut(player->getCutType())) {
            attackPos = player->current.pos;
            radius = 200.0f;
        } else {
            MTXCopy(player->getLeftItemMatrix(), mDoMtx_stack_c::get());
            mDoMtx_stack_c::multVecZero(&attackPos);
            attackPos.y -= 50.0f;
        }

        cXyz delta = i_this->field_0x18 - attackPos;
        if (player->getCutAtFlg() == 0 || delta.abs() >= radius) {
            return;
        }

        if (player->checkWolf()) {
            hit = i_this->field_0x50 == 2 ? 2 : 1;
        } else if (i_this->field_0x50 == 1 || radius > 100.0f) {
            hit = 1;
        }
        if (hit != 0 && hitDelta != NULL) {
            *hitDelta = delta;
        }
    });
    return hit;
}

}  // namespace
#endif

static int daE_Bug_Draw(e_bug_class* i_this) {
    bug_s* bugs = i_this->Bug_s;

    for (int i = 0; i < i_this->bug_num; i++, bugs++) {
        if (bugs->field_0x50 == 4) {
            if ((bugs->field_0x8 & 1) != 0) {
                dComIfGp_entrySimpleModel(bugs->field_0x0, l_roomNo);
            } else {
                dComIfGp_entrySimpleModel(bugs->field_0x4, l_roomNo);
            }
        } else if (bugs->field_0x50 >= 1) {
            if ((bugs->field_0x52 & 2) != 0) {
                dComIfGp_entrySimpleModel(bugs->field_0x0, l_roomNo);
            } else {
                dComIfGp_entrySimpleModel(bugs->field_0x4, l_roomNo);
            }
        }
    }

    return 1;
}

static int simple_bg_check(bug_s* i_this, int param_2) {
    dBgS_LinChk lin_chk;
    cXyz start, end, spc0, spcc;

    spcc.y = 0.0f;
    int iVar1 = 0;
    f32 fVar1 = (i_this->field_0xc - i_this->field_0x18).abs() + 5.0f;
    cMtx_YrotS(*calc_mtx, i_this->field_0x3c.y);

    static f32 c_x[4] = {0.0f, 1.0f, -1.0f, 0.0f};
    static f32 c_z[4] = {2.0f, 0.0f, 0.0f, -2.0f};
    for (int i = 0; i < param_2 + 3; i++) {
        start = i_this->field_0x18;
        start.y += 20.0f;
        spcc.x = fVar1 * c_x[i];
        spcc.z = fVar1 * c_z[i];
        MtxPosition(&spcc, &spc0);
        end = start + spc0;
        lin_chk.Set(&start, &end, NULL);

        if (dComIfG_Bgsp().LineCross(&lin_chk)) {
            i_this->field_0x18.x -= spc0.x;
            i_this->field_0x18.z -= spc0.z;
            iVar1 = i + 1;
        }
    }

    return iVar1;
}

static void bug_mtxset(bug_s* i_this) {
    mDoMtx_stack_c::transS(i_this->field_0x18.x, i_this->field_0x18.y, i_this->field_0x18.z);
    mDoMtx_stack_c::XrotM(i_this->field_0x44);
    mDoMtx_stack_c::ZrotM(i_this->field_0x46);
    mDoMtx_stack_c::YrotM(i_this->field_0x3c.y);
    mDoMtx_stack_c::XrotM(i_this->field_0x3c.x);
    mDoMtx_stack_c::scaleM(l_HIO.field_0x8 * i_this->field_0x28, l_HIO.field_0x8 * i_this->field_0x28, l_HIO.field_0x8 * i_this->field_0x28);

    if ((i_this->field_0x52 & 2) != 0) {
        i_this->field_0x0->setBaseTRMtx(mDoMtx_stack_c::get());
    } else {
        i_this->field_0x4->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

static void bug_mtxset_stick(bug_s* i_this) {
    cXyz sp2c;
    mDoMtx_stack_c::transS(i_this->field_0x18.x, i_this->field_0x18.y, i_this->field_0x18.z);
    mDoMtx_stack_c::YrotM(i_this->field_0x3c.y);
    sp2c = i_this->field_0x18 - i_this->field_0xc;

    s16 sVar1 = cM_atan2s(JMAFastSqrt(sp2c.x * sp2c.x + sp2c.z * sp2c.z), sp2c.y);
    if (i_this->field_0x4a < 0) {
        sVar1 *= -1;
    }

    cLib_addCalcAngleS2(&i_this->field_0x3c.z, sVar1, 2, 0x1000);
    mDoMtx_stack_c::ZrotM(i_this->field_0x3c.z);
    mDoMtx_stack_c::XrotM(-0x4000);
    mDoMtx_stack_c::scaleM(l_HIO.field_0xc * i_this->field_0x28, l_HIO.field_0xc * i_this->field_0x28, l_HIO.field_0xc * i_this->field_0x28);
    mDoMtx_stack_c::transM(0.0f, 2.0f, 0.0f);

    if ((i_this->field_0x52 & 2) != 0) {
        i_this->field_0x0->setBaseTRMtx(mDoMtx_stack_c::get());
    } else {
        i_this->field_0x4->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

#if TARGET_PC
static void bug_mtxset_stickW(bug_s* i_this, daPy_py_c* player) {
#else
static void bug_mtxset_stickW(bug_s* i_this) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif
    cXyz sp38, sp44, sp50;

    sp38 = i_this->field_0x18 - i_this->field_0xc;
    s16 sVar1 = cM_atan2s(JMAFastSqrt(sp38.x * sp38.x + sp38.z * sp38.z), sp38.y);
    if (i_this->field_0x4a < 0) {
        sVar1 *= -1;
    }

#if TARGET_PC
    MTXCopy(static_cast<daAlink_c*>(player)->getModelJointMtx(1), *calc_mtx);
#else
    MTXCopy(daPy_getLinkPlayerActorClass()->getModelJointMtx(1), *calc_mtx);
#endif
    sp38.set(0.0f, 0.0f, 0.0f);
    MtxPosition(&sp38, &sp44);
    sp50 = i_this->field_0x18 - sp44;
    mDoMtx_stack_c::transS(sp44.x, sp44.y, sp44.z);
    mDoMtx_stack_c::YrotM(player->shape_angle.y);
    mDoMtx_stack_c::XrotM(-14000);
    mDoMtx_stack_c::transM(sp50.x, sp50.y, sp50.z);
    mDoMtx_stack_c::YrotM(i_this->field_0x3c.y);
    cLib_addCalcAngleS2(&i_this->field_0x3c.z, sVar1, 2, 0x1000);
    mDoMtx_stack_c::ZrotM(i_this->field_0x3c.z);
    mDoMtx_stack_c::XrotM(-0x4000);
    mDoMtx_stack_c::scaleM(l_HIO.field_0xc * i_this->field_0x28, l_HIO.field_0xc * i_this->field_0x28, l_HIO.field_0xc * i_this->field_0x28);
    mDoMtx_stack_c::transM(0.0f, 2.0f, 0.0f);

    if ((i_this->field_0x52 & 2) != 0) {
        i_this->field_0x0->setBaseTRMtx(mDoMtx_stack_c::get());
    } else {
        i_this->field_0x4->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

static void bug_mtxset_fail(bug_s* i_this) {
    mDoMtx_stack_c::transS(i_this->field_0x18.x, i_this->field_0x18.y, i_this->field_0x18.z);
    mDoMtx_stack_c::XrotM(i_this->field_0x44);
    mDoMtx_stack_c::ZrotM(i_this->field_0x46);
    mDoMtx_stack_c::YrotM(i_this->field_0x3c.y);
    mDoMtx_stack_c::XrotM(i_this->field_0x3c.x);
    mDoMtx_stack_c::scaleM(l_HIO.field_0x8 * i_this->field_0x28, l_HIO.field_0x8 * i_this->field_0x28, l_HIO.field_0x8 * i_this->field_0x28);
    mDoMtx_stack_c::transM(0.0f, -4.0f, 0.0f);

    if ((i_this->field_0x8 & 1) != 0) {
        i_this->field_0x0->setBaseTRMtx(mDoMtx_stack_c::get());
    } else {
        i_this->field_0x4->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

static void bug_ground_ang_set(bug_s* i_this) {
    dBgS_LinChk lin_chk;
    cXyz sp8c, end, cross;

    cXyz start(i_this->field_0x18);
    start.y += 30.0f;

    mDoMtx_stack_c::transS(i_this->field_0x18.x, i_this->field_0x18.y, i_this->field_0x18.z);
    mDoMtx_stack_c::transM(5.0f, -30.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&end);
    mDoMtx_stack_c::transM(-10.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&cross);

    s8 sVar1 = 0;
    lin_chk.Set(&start, &end, NULL);

    if (dComIfG_Bgsp().LineCross(&lin_chk)) {
        end = lin_chk.GetCross();
        lin_chk.Set(&start, &cross, NULL);

        if (dComIfG_Bgsp().LineCross(&lin_chk)) {
            cross = lin_chk.GetCross();
            sVar1 = 1;
        }
    }

    if (sVar1) {
        sp8c = end - cross;
        i_this->field_0x46 = cM_atan2s(sp8c.y, JMAFastSqrt(sp8c.x * sp8c.x + sp8c.z * sp8c.z));
    }

    mDoMtx_stack_c::transM(5.0f, 0.0f, 5.0f);
    mDoMtx_stack_c::multVecZero(&end);
    mDoMtx_stack_c::transM(0.0f, 0.0f, -10.0f);
    mDoMtx_stack_c::multVecZero(&cross);
    sVar1 = 0;
    lin_chk.Set(&start, &end, NULL);

    if (dComIfG_Bgsp().LineCross(&lin_chk)) {
        end = lin_chk.GetCross();
        lin_chk.Set(&start, &cross, NULL);

        if (dComIfG_Bgsp().LineCross(&lin_chk)) {
            cross = lin_chk.GetCross();
            sVar1 = 1;
        }
    }

    if (sVar1) {
        sp8c = end - cross;
        i_this->field_0x44 = -cM_atan2s(sp8c.y, JMAFastSqrt(sp8c.x * sp8c.x + sp8c.z * sp8c.z));
    }
}

static int bug_action(e_bug_class* a_this, bug_s* i_this) {
    int rv = 0;
    i_this->field_0x18.x += i_this->field_0x30.x;
    i_this->field_0x18.y += i_this->field_0x30.y;
    i_this->field_0x18.z += i_this->field_0x30.z;
    
    i_this->field_0x30.y -= 3.0f;
    if (i_this->field_0x30.y < -60.0f) {
        i_this->field_0x30.y = -60.0f;
    }

    cXyz sp24(i_this->field_0x18.x, i_this->field_0x18.y + 70.0f, i_this->field_0x18.z);

    if (fopAcM_gc_c::gndCheck(&sp24)) {
        if (i_this->field_0x18.y <= fopAcM_gc_c::getGroundY()) {
            i_this->field_0x18.y = fopAcM_gc_c::getGroundY();
            i_this->field_0x30.y = -i_this->field_0x24 - 0.5f;

            if ((i_this->field_0x52 & 7) == 0) {
                bug_ground_ang_set(i_this);
            }

            rv = 1;
        }
    }

    return rv;
}

static cXyz at_pos;

static f32 at_size;

static s8 data_80697EAC;

#if TARGET_PC
static void bug_stick(bug_s* i_this, daPy_py_c* player) {
#else
static void bug_stick(bug_s* i_this) {
    daPy_py_c* player = daPy_getLinkPlayerActorClass();
#endif
    cXyz sp30, sp3c, sp48;
    
    MTXCopy(player->getModelJointMtx(1), *calc_mtx);
    sp30.set(0.0f, 0.0f, 0.0f);
    MtxPosition(&sp30, &sp48);

    if ((i_this->field_0x52 & 15) == 0) {
        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x4a = cM_rndF(1000.0f) + 1000.0f;
        } else {
            i_this->field_0x4a = -(cM_rndF(1000.0f) + 1000.0f);
        }

        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x4e = cM_rndF(200.0f) + 600.0f;
        }
    }

    i_this->field_0x48 += i_this->field_0x4a;
    i_this->field_0x4c += i_this->field_0x4e;
    f32 fVar1 = cM_ssin(i_this->field_0x4c);
    cMtx_YrotS(*calc_mtx, i_this->field_0x48);
    sp30.x = 0.0f;
    sp30.y = fVar1 * 40.0f;
    sp30.z = 7.0f * fabsf(fVar1) + 13.0f;

    if (fabsf(sp30.y) > 30.0f) {
        sp30.z *= 0.6f;
    }

    MtxPosition(&sp30, &sp3c);
    i_this->field_0x18 = sp48 + sp3c;
    i_this->field_0x3c.y = i_this->field_0x48 + 0x8000;

    if ((i_this->field_0x52 & 15) == 0 &&
        (player->checkFrontRoll() || player->checkMetamorphose() ||
         player->eventInfo.checkCommandDoor() || data_80697EAC != 0))
    {
        i_this->field_0x50 = 1;
        i_this->field_0x3c.y = i_this->field_0x48;
        i_this->field_0x30.y = cM_rndF(5.0f) + 30.0f;
        i_this->field_0x24 = cM_rndF(3.0f) + 9.0f;

        i_this->mSound.startSound(Z2SE_EN_BUG_JUMP, 0, -1);
        i_this->field_0x54[2] = cM_rndF(20.0f) + 20.0f;
    }
}

#if TARGET_PC
static void bug_stickW(bug_s* i_this, daPy_py_c* player) {
#else
static void bug_stickW(bug_s* i_this) {
#endif
    cXyz sp34, sp40, sp4c;

#if TARGET_PC
    MTXCopy(static_cast<daAlink_c*>(player)->getModelJointMtx(1), *calc_mtx);
#else
    MTXCopy(daPy_getLinkPlayerActorClass()->getModelJointMtx(1), *calc_mtx);
#endif
    sp34.set(0.0f, 0.0f, 0.0f);
    MtxPosition(&sp34, &sp4c);

    if ((i_this->field_0x52 & 15) == 0) {
        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x4a = cM_rndF(1000.0f) + 1000.0f;
        } else {
            i_this->field_0x4a = -(cM_rndF(1000.0f) + 1000.0f);
        }

        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x4e = cM_rndF(200.0f) + 600.0f;
        }
    }

    i_this->field_0x48 += i_this->field_0x4a;
    i_this->field_0x4c += i_this->field_0x4e;
    f32 fVar1 = cM_ssin(i_this->field_0x4c);
    cMtx_YrotS(*calc_mtx, i_this->field_0x48);
    sp34.x = 0.0f;
    sp34.y = (fVar1 * 41.0f - 5.0f) - 30.0f;
    sp34.z = -13.0f * fabsf(fVar1) + 13.0f + 10.0f;
    MtxPosition(&sp34, &sp40);
    i_this->field_0x18 = sp4c + sp40;
    i_this->field_0x3c.y = i_this->field_0x48 + 0x8000;
}

static void wind_move(bug_s* i_this) {
    s16 sVar1 = i_this->field_0x52 * -0x1700;
    i_this->field_0x3c.x -= 0xD00;
    i_this->field_0x3c.y += 0x700;
    i_this->field_0x30.x = cM_ssin(sVar1) * 25.0f;
    i_this->field_0x30.z = cM_scos(sVar1) * 25.0f;
    i_this->field_0x30.y = 20.0f;
    i_this->field_0x18 += i_this->field_0x30;

    if (i_this->field_0x54[0] == 0 || simple_bg_check(i_this, 1) != 0) {
        i_this->field_0x50 = 1;
        i_this->field_0x3c.y = sVar1;
        i_this->field_0x24 = cM_rndF(5.0f) + 8.0f;
    }
}

static void bug_fail(e_bug_class* a_this, bug_s* i_this) {
    i_this->field_0x53 = bug_action(a_this, i_this);

    if (i_this->field_0x51 == 0 && simple_bg_check(i_this, 1) != 0) {
        i_this->field_0x51 = 1;
        i_this->field_0x18 = i_this->field_0xc;
        i_this->field_0x30.x = 0.0f;
        i_this->field_0x30.z = 0.0f;
    }

    if (i_this->field_0x53 == 0) {
        i_this->field_0x3c.x += 0x2000;
        i_this->field_0x3c.y += 0x1300;
    } else {
        i_this->field_0x30.set(0.0f, 0.0f, 0.0f);
        i_this->field_0x3c.x = -0x8000;
    }

    if (i_this->field_0x54[0] == 0) {
        cLib_addCalc0(&i_this->field_0x28, 1.0f, 0.1f);
        if (i_this->field_0x28 < 0.01f) {
            i_this->field_0x50 = 0;
        }
    }
}

static void damage_check(e_bug_class* a_this, bug_s* i_this) {
    fopAc_ac_c* actor = (fopAc_ac_c*)&a_this->actor;
#if !TARGET_PC
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#endif
    cXyz sp4c, sp58;
    f32 fVar1 = 70.0f;

#if TARGET_PC
    // Co-op: every active player's live cut/body matrix may damage an internal swarm mite.
    s8 sVar1 = coOpBugCheckPlayerDamage(i_this, &sp4c);
#else
    if (daPy_py_c::checkNowWolf()) {
        sp58 = player->current.pos;
    } else {
        if (daPy_getLinkPlayerActorClass()->getCutType() == daPy_py_c::CUT_TYPE_TURN_RIGHT || daPy_getLinkPlayerActorClass()->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT ||
            daPy_getLinkPlayerActorClass()->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT || daPy_getLinkPlayerActorClass()->getCutType() == daPy_py_c::CUT_TYPE_TURN_LEFT) {
            sp58 = player->current.pos;
            fVar1 = 200.0f;
        } else {
            MTXCopy(player->getLeftItemMatrix(), mDoMtx_stack_c::get());
            mDoMtx_stack_c::multVecZero(&sp58);
            sp58.y -= 50.0f;
        }
    }

    sp4c = i_this->field_0x18 - sp58;
    s8 sVar1 = 0;

    if (daPy_getPlayerActorClass()->getCutAtFlg() != 0 && sp4c.abs() < fVar1) {
        if (daPy_py_c::checkNowWolf()) {
            if (i_this->field_0x50 == 2) {
                sVar1 = 2;
            } else {
                sVar1 = 1;
            }
        } else if (i_this->field_0x50 == 1 || fVar1 > 100.0f) {
            sVar1 = 1;
        }
    }
#endif

    if (data_80697E8D != 0) {
        sp4c = i_this->field_0x18 - at_pos;

        if (data_80697E8D == 1) {
            if (sp4c.y < 50.0f && sp4c.y > -400.0f && JMAFastSqrt(sp4c.x * sp4c.x + sp4c.z * sp4c.z) < 50.0f) {
                sVar1 = 2;
            }
        } else if (data_80697E8D == 2) {
            if (sp4c.abs() < at_size) {
                sVar1 = 1;
            }
        }
    }
    
    if (sVar1 != 0){
        if (sVar1 == 1) {
            i_this->field_0x50 = 4;
            i_this->field_0x51 = 0;
            i_this->field_0x54[0] = cM_rndF(15.0f) + 15.0f + 25.0f;
            i_this->mSound.startSound(Z2SE_EN_BUG_DIE, 0, -1);
            cMtx_YrotS(*calc_mtx, cM_atan2s(sp4c.x, sp4c.z));
            sp4c.x = 0.0f;

            if (data_80697E8D == 2) {
                sp4c.y = cM_rndF(10.0f) + 35.0f;
                sp4c.z = cM_rndF(10.0f) + 35.0f;
            } else {
                sp4c.y = cM_rndF(10.0f) + 20.0f;
                sp4c.z = cM_rndF(10.0f) + 20.0f;
            }

            MtxPosition(&sp4c, &i_this->field_0x30);
            i_this->field_0x53 = 0;
            i_this->field_0x3c.x = cM_rndF(65536.0f);
            i_this->field_0x3c.z = cM_rndF(65536.0f);
            cXyz sp64(0.35f, 0.35f, 0.35f);
            dComIfGp_setHitMark(1, actor, &i_this->field_0x18, NULL, &sp64, 0);
        } else {
            i_this->field_0x50 = 3;
            i_this->field_0x54[0] = cM_rndF(10.0f) + 15.0f;
            i_this->field_0x3c.y = cM_rndF(65536.0f);
        }
    }
}

#if TARGET_PC
static void set_wait(e_bug_class* a_this, bug_s* i_this, CoOpBugTargetState* target) {
    daPy_py_c* player = coOpBugTargetPlayer(*target);
#else
static void set_wait(e_bug_class* a_this, bug_s* i_this) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif
    cXyz sp40;
    s8 sVar1 = 0;

    switch (i_this->field_0x51) {
        case 0:
            if (a_this->bitSw != 0xFF) {
                if (dComIfGs_isSwitch(a_this->bitSw, l_roomNo)) {
                    sVar1 = 1;
                }
            } else {
#if TARGET_PC
                sp40 = i_this->field_0x18 - player->current.pos;
                if (target->state.available && sp40.abs() < a_this->field_0x57c) {
                    sVar1 = 1;
                }
#else
                sp40 = i_this->field_0x18 - player->current.pos;
                if (sp40.abs() < a_this->field_0x57c) {
                    sVar1 = 1;
                }
#endif
            }
            break;

        case 1:
            if (i_this->field_0x54[0] == 0) {
                i_this->field_0x50 = 1;
                i_this->field_0x51 = 0;
            }
            break;
    }

    if (sVar1) {
        i_this->field_0x54[0] = i_this->field_0x8 * 3.0f + cM_rndF(2.0f);
        i_this->field_0x51 = 1;
        sp40 = i_this->field_0x18 - player->current.pos;
        i_this->field_0x3c.y = cM_rndFX(65536.0f);
    }
}

#if TARGET_PC
static void normal_move(e_bug_class* a_this, bug_s* i_this, daPy_py_c* player,
                        const CoOpBugTargetState& target) {
#else
static void normal_move(e_bug_class* a_this, bug_s* i_this) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif
    cXyz sp68, sp74;

    if (i_this->field_0x53 != 0) {
        if ((i_this->field_0x52 & 15) == 0) {
            i_this->field_0x4a = cM_rndF(1000.0f) + 700.0f;
            i_this->field_0x24 = cM_rndFX(0.5f) + 5.0f;
        }

        s16 sVar1 = cM_ssin(i_this->field_0x48) * 3000.0f;
        i_this->field_0x48 += i_this->field_0x4a;
        sp74.x = player->current.pos.x - i_this->field_0x18.x;
        sp74.z = player->current.pos.z - i_this->field_0x18.z;

        if (data_80697EAC != 0 && JMAFastSqrt(sp74.x * sp74.x + sp74.z * sp74.z) < 200.0f) {
            sVar1 -= 0x8000;
        }

        cLib_addCalcAngleS2(&i_this->field_0x3c.y, i_this->field_0x42 + (sVar1 + cM_atan2s(sp74.x, sp74.z)), 2, 0x800);
        i_this->field_0x3c.x = 0;
    } else {
        ANGLE_ADD(i_this->field_0x3c.x, (i_this->field_0x8 << 1) + 0xE00);
    }

    i_this->field_0x30.x = i_this->field_0x24 * cM_ssin(i_this->field_0x3c.y);
    i_this->field_0x30.z = i_this->field_0x24 * cM_scos(i_this->field_0x3c.y);
    i_this->field_0x53 = bug_action(a_this, i_this);

    if (i_this->field_0x54[1] == 1) {
        i_this->field_0x42 = 0;
    }

    sp74 = player->current.pos - i_this->field_0x18;
    f32 fVar1 = JMAFastSqrt(sp74.x * sp74.x + sp74.z * sp74.z);
    f32 fVar2 = sp74.y;

    if ((i_this->field_0x52 & 1) != 0) {
        f32 fVar3 = sp74.abs();

        if (i_this->field_0x53 != 0 && data_80697EAC == 0 && i_this->field_0x54[2] == 0 && fVar3 < 140.0f) {
            s16 sVar2 = cM_atan2s(sp74.x, sp74.z);
            s16 sVar3 = i_this->field_0x3c.y - sVar2;
            
            if (sVar3 < 0x1000 && sVar3 > -0x1000) {
                i_this->field_0x30.y = cM_rndF(5.0f) + 25.0f;
                i_this->field_0x24 = 9.0f;
                i_this->field_0x53 = 0;
                i_this->field_0x3c.y = sVar2;
                i_this->mSound.startSound(Z2SE_EN_BUG_JUMP, 0, -1);
            }
        }

        f32 fVar4 = fVar3 * 0.10000000149011612f;
        if (fVar4 > 100.0f) {
            fVar4 = 100.0f;
        } else if (fVar4 < 20.0f) {
            fVar4 = 20.0f;
        }

        cLib_addCalc2(&i_this->field_0x2c, fVar4, 0.2f, 2.0f);
        int iVar1 = i_this->field_0x8 + 1;
        for (int i = iVar1; i < a_this->bug_num; i++) {
            sp68.x = a_this->Bug_s[i].field_0x18.x - i_this->field_0x18.x;
            sp68.z = a_this->Bug_s[i].field_0x18.z - i_this->field_0x18.z;
            fVar4 = JMAFastSqrt(sp68.x * sp68.x + sp68.z * sp68.z);
            iVar1 = i;

            if (fVar4 < i_this->field_0x2c) {
                cMtx_YrotS(*calc_mtx, cM_atan2s(-sp68.x, -sp68.z));
                sp68.x = 0.0f;
                sp68.y = 0.0f;
                sp68.z = 0.4f;
                MtxPosition(&sp68, &sp74);
                i_this->field_0x18.x += sp74.x;
                i_this->field_0x18.z += sp74.z;
                iVar1 = i;
            }
        }
    }

    if (i_this->field_0x30.y <= 0.0f && data_80697EAC == 0 &&
#if TARGET_PC
        !target.state.status0_0x100 &&
#else
        !dComIfGp_checkPlayerStatus0(0, 0x100) &&
#endif
        fVar1 < 40.0f && fVar2 <= 0.0f && fVar2 >= -150.0f)
    {
        i_this->field_0x50 = 2;

        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x4a = cM_rndF(1000.0f) + 1000.0f;
        } else {
            i_this->field_0x4a = -(cM_rndF(1000.0f) + 1000.0f);
        }

        i_this->field_0x4e = cM_rndF(300.0f) + 600.0f;
    }

    int iVar2 = simple_bg_check(i_this, 0);

    if (i_this->field_0x54[1] == 0 && iVar2 >= 2) {
        if (iVar2 == 3) {
            i_this->field_0x42 = 0x1000;
        } else {
            i_this->field_0x42 = -0x1000;
        }

        i_this->field_0x54[1] = cM_rndF(30.0f) + 30.0f;
    }
}

static void bug_control(e_bug_class* a_this) {
    fopAc_ac_c* actor = (fopAc_ac_c*)&a_this->actor;
#if TARGET_PC
    CoOpBugTargetState target;
    coOpPrepareBugTarget(a_this, &target);
    daPy_py_c* player = coOpBugTargetPlayer(target);
    // Co-op: lantern/door repulsion belongs to the retained player the swarm is approaching.
    data_80697EAC =
        player->getKandelaarFlamePos() != NULL || player->eventInfo.checkCommandDoor();
#else
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#endif
    cXyz sp1c, sp28;
    bug_s* i_this = a_this->Bug_s;
    u8 sVar1 = 0;
    u8 sVar2 = 0;
    u8 uVar1 = 0;

    for (int i = 0; i < a_this->bug_num; i++, i_this++) {
        if (i_this->field_0x50 != 0) {
            if (i_this->field_0x18.y < actor->home.pos.y - 2000.0f) {
                i_this->field_0x50 = 0;
            }

            sVar1++;
            i_this->field_0xc = i_this->field_0x18;
            actor->current.pos = i_this->field_0x18;

            for (int j = 0; j < 3; j++) {
                if (i_this->field_0x54[j] != 0) {
                    i_this->field_0x54[j]--;
                }
            }

            i_this->field_0x52++;

            if (i_this->field_0x50 == -1) {
#if TARGET_PC
                set_wait(a_this, i_this, &target);
                player = coOpBugTargetPlayer(target);
#else
                set_wait(a_this, i_this);
#endif
                bug_mtxset(i_this);
            } else if (i_this->field_0x50 == 1) {
#if TARGET_PC
                normal_move(a_this, i_this, player, target);
#else
                normal_move(a_this, i_this);
#endif

                if (i_this->field_0x53 != 0) {
                    sVar2++;
                }

                bug_mtxset(i_this);
                damage_check(a_this, i_this);
            } else if (i_this->field_0x50 == 2) {
#if TARGET_PC
                if (target.state.wolf) {
#else
                if (daPy_py_c::checkNowWolf()) {
#endif
#if TARGET_PC
                    bug_stickW(i_this, player);
                    bug_mtxset_stickW(i_this, player);
#else
                    bug_stickW(i_this);
                    bug_mtxset_stickW(i_this);
#endif
                } else {
#if TARGET_PC
                    bug_stick(i_this, player);
#else
                    bug_stick(i_this);
#endif
                    bug_mtxset_stick(i_this);
                }

                uVar1++;
                damage_check(a_this, i_this);
            } else if (i_this->field_0x50 == 3) {
                wind_move(i_this);
                bug_mtxset(i_this);
                damage_check(a_this, i_this);
            } else if (i_this->field_0x50 == 4) {
                bug_fail(a_this, i_this);
                bug_mtxset_fail(i_this);
            }

            i_this->mSound.framework(0, dComIfGp_getReverb(l_roomNo));
        }
    }

    if (sVar1 == 0) {
        fopAcM_delete(actor);
        OS_REPORT("E_BUG DELETED \n");
    } else {
        if (sVar2 != 0) {
            a_this->mSound.playBeeGroupSound(Z2SE_EN_BUG_WALK_GRD, sVar2);
        }

        if (uVar1 != 0) {
            a_this->mSound.playBeeGroupSound(Z2SE_EN_BUG_WALK_BODY, uVar1);
        }

        a_this->mSound.framework(0, dComIfGp_getReverb(fopAcM_GetRoomNo(actor)));
    }

    if (uVar1 > 10) {
        player->onHeavyStateMidnaPanic();
    }
}

static void* s_boom_sub(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor) && fopAcM_GetName(i_actor) == fpcNm_BOOMERANG_e && daPy_py_c::checkBoomerangCharge() && fopAcM_GetParam(i_actor) == 1) {
        data_80697E8D = 1;
        at_pos = ((fopAc_ac_c*)i_actor)->current.pos;
        return i_actor;
    }

    return NULL;
}

static void* s_bomb_sub(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor) && dBomb_c::checkBombActor((fopAc_ac_c*)i_actor) && ((daNbomb_c*)i_actor)->checkExplodeNow()) {
        data_80697E8D = 2;
        at_pos = ((fopAc_ac_c*)i_actor)->current.pos;
        at_size = 300.0f;
        return i_actor;
    }

    return NULL;
}

static int daE_Bug_Execute(e_bug_class* i_this) {
#if TARGET_PC
    // Co-op: attached-swarm door/metamorphose release must keep executing for its retained owner.
    daPy_py_c* player = coOpBugRetainedTargetPlayer(i_this);
#else
    daPy_py_c* player = daPy_getPlayerActorClass();
#endif

    if (!player->checkMetamorphose()) {
        if (!player->eventInfo.checkCommandDoor() && dComIfGp_event_runCheck()) {
            return 1;
        }
    }

    i_this->field_0x580++;
    data_80697E8D = 0;
#if TARGET_PC
    const dusk::coop::item_awareness::ItemAwarenessResult boomerang =
        dusk::coop::item_awareness::findActiveBoomerang(&i_this->actor, "e_bug.boomerang");
    if (boomerang.found && boomerang.itemActor != NULL) {
        // Co-op: Gale Boomerang wind samples the active owner-local item actor.
        data_80697E8D = 1;
        at_pos = boomerang.itemActor->current.pos;
    }
#else
    fpcM_Search(s_boom_sub, i_this);
#endif
    fpcM_Search(s_bomb_sub, i_this);

#if TARGET_PC
    bool foundSpinnerAttack = false;
    dusk::coop::forEachActivePlayer(
        [&](dusk::coop::PlayerSlot, fopAc_ac_c* actor) {
            if (foundSpinnerAttack || actor == NULL) {
                return;
            }
            daPy_py_c* activePlayer = static_cast<daPy_py_c*>(actor);
            if (activePlayer->checkSpinnerRide() &&
                activePlayer->checkSpinnerTriggerAttack())
            {
                // Co-op: any active player's Spinner attack can strike this shared swarm.
                data_80697E8D = 2;
                at_pos = activePlayer->current.pos;
                at_size = 120.0f;
                foundSpinnerAttack = true;
            }
        });
#else
    if (daPy_getPlayerActorClass()->checkSpinnerRide()) {
        if (daPy_getPlayerActorClass()->checkSpinnerTriggerAttack()) {
            data_80697E8D = 2;
            at_pos = player->current.pos;
            at_size = 120.0f;
        }
    }
#endif

    if ((i_this->field_0x580 & 1) != 0) {
#if TARGET_PC
        bool foundIronBall = false;
        dusk::coop::forEachActivePlayer(
            [&](dusk::coop::PlayerSlot, fopAc_ac_c* actor) {
                if (foundIronBall || actor == NULL) {
                    return;
                }
                daPy_py_c* activePlayer = static_cast<daPy_py_c*>(actor);
                cXyz* pos = activePlayer->getIronBallCenterPos();
                if (pos != NULL && (activePlayer->current.pos - *pos).abs() > 200.0f &&
                    !activePlayer->checkIronBallReturn() &&
                    !activePlayer->checkIronBallGroundStop())
                {
                    // Co-op: Ball and Chain proximity comes from its concrete owning player.
                    data_80697E8D = 2;
                    at_pos = *pos;
                    at_size = 130.0f;
                    foundIronBall = true;
                }
            });
#else
        cXyz* pos = player->getIronBallCenterPos();
        if (pos != NULL && (player->current.pos - *pos).abs() > 200.0f && !daPy_getPlayerActorClass()->checkIronBallReturn() && !daPy_getPlayerActorClass()->checkIronBallGroundStop()) {
            data_80697E8D = 2;
            at_pos = *pos;
            at_size = 130.0f;
        }
#endif
    }

#if !TARGET_PC
    if (daPy_getPlayerActorClass()->getKandelaarFlamePos() != NULL || daPy_getPlayerActorClass()->eventInfo.checkCommandDoor()) {
        data_80697EAC = 1;
    } else {
        data_80697EAC = 0;
    }
#endif

    bug_control(i_this);
    return 1;
}

static int daE_Bug_IsDelete(e_bug_class* i_this) {
    return 1;
}

static int daE_Bug_Delete(e_bug_class* i_this) {
    fopAc_ac_c* a_this = &i_this->actor;

#if TARGET_PC
    // Co-op: the swarm's retained target must not survive actor deletion or pointer reuse.
    dusk::coop::clearAllEnemyTargets(a_this);
#endif
    static u32 const l_bmdidx[2] = {BMDG_MU04, BMDG_MU05};
    if (i_this->field_0x7dad != 0) {
        for (u32 i = 0; i < 2; i++) {
            J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_bug", l_bmdidx[i]);
            dComIfGp_removeSimpleModel(modelData, fopAcM_GetRoomNo(a_this));
        }
    }

    dComIfG_resDelete(&i_this->mPhase, "E_bug");

    if (i_this->field_0x7dac != 0) {
        hio_set = 0;
    }

    if (a_this->heap != NULL) {
        i_this->mSound.deleteObject();
        for (int i = 0; i < i_this->bug_num; i++) {
            i_this->Bug_s[i].mSound.deleteObject();
        }
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* a_this) {
    e_bug_class* i_this = (e_bug_class*)a_this;
    
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes("E_bug", BMDG_MU04));
    JUT_ASSERT(1322, modelData != NULL);

    J3DModelData* modelData2 = static_cast<J3DModelData*>(dComIfG_getObjectRes("E_bug", BMDG_MU05));
    JUT_ASSERT(1327, modelData2 != NULL);

    for (int i = 0; i < i_this->bug_num; i++) {
        i_this->Bug_s[i].field_0x0 = mDoExt_J3DModel__create(modelData, 0x20000, 0x11000084);
        if (i_this->Bug_s[i].field_0x0 == NULL) {
            return 0;
        }
        
        i_this->Bug_s[i].field_0x4 = mDoExt_J3DModel__create(modelData2, 0x20000, 0x11000084);
        if (i_this->Bug_s[i].field_0x4 == NULL) {
            return 0;
        }
    }

    return 1;
}

static cPhs_Step daE_Bug_Create(fopAc_ac_c* a_this) {
    e_bug_class* i_this = (e_bug_class*)a_this;

    fopAcM_ct(a_this, e_bug_class);

    cPhs_Step phase = dComIfG_resLoad(&i_this->mPhase, "E_bug");
    if (phase == cPhs_COMPLEATE_e) {
        OS_REPORT("E_BUG PARAM %x\n", fopAcM_GetParam(a_this));
        i_this->field_0x570 = fopAcM_GetParam(a_this);
        i_this->bug_num = i_this->field_0x570 + 1;

        if (i_this->bug_num > 0x100) {
            i_this->bug_num = 0x100;
        }

        if (strcmp(dComIfGp_getStartStageName(), "T_ENEMY") == 0) {
            i_this->bug_num = 0x100;
        }

        OS_REPORT("E_BUG//////////////E_BUG SET 1 !!\n");
        if (!fopAcM_entrySolidHeap(a_this, useHeapInit, 0x4b000)) {
            OS_REPORT("//////////////E_BUG SET NON !!\n");
            return cPhs_ERROR_e;
        }

        OS_REPORT("//////////////E_BUG SET 2 !!\n");
        if (hio_set == 0) {
            i_this->field_0x7dac = 1;
            hio_set = 1;
            l_HIO.field_0x4 = -1;
        }

        i_this->bitSw = (fopAcM_GetParam(a_this) & 0xFF0000) >> 16;
        i_this->field_0x578 = fopAcM_GetParam(a_this) >> 24;
        i_this->field_0x57c = ((fopAcM_GetParam(a_this) & 0xFF00) >> 8) * 100.0f;

        u8 uVar1 = a_this->home.angle.z;
        a_this->home.angle.z = 0;
        a_this->current.angle.z = 0;

        for (int i = 0; i < i_this->bug_num; i++) {
            i_this->Bug_s[i].field_0x8 = i;
            i_this->Bug_s[i].field_0x50 = -1;
            i_this->Bug_s[i].field_0x18 = a_this->home.pos;
            i_this->Bug_s[i].field_0x52 = i;
            i_this->Bug_s[i].field_0x28 = cM_rndFX(0.1f) + 1.0f;
            i_this->Bug_s[i].field_0x48 = cM_rndF(65536.0f);
            i_this->Bug_s[i].field_0x4c = cM_rndF(65536.0f);
            i_this->Bug_s[i].mSound.init(&i_this->Bug_s[i].field_0x18, 1);
        }

        if (uVar1 != 0) {
            dPath* roomPath = dPath_GetRoomPath(uVar1, fopAcM_GetRoomNo(a_this));
            if (roomPath != NULL) {
                dPnt* mPnts = roomPath->m_points;
                if (roomPath->m_num >= 1 && roomPath->m_num <= 4) {
                    int iVar1 = i_this->bug_num / (roomPath->m_num + 1);
                    for (int i = 0; i < roomPath->m_num; i++, mPnts++) {
                        for (int j = iVar1 * (i + 1); j < iVar1 * (i + 2); j++) {
                            i_this->Bug_s[j].field_0x18.x = mPnts->m_position.x;
                            i_this->Bug_s[j].field_0x18.y = mPnts->m_position.y;
                            i_this->Bug_s[j].field_0x18.z = mPnts->m_position.z;
                        }
                    }
                }
            }
        }

        daE_Bug_Execute(i_this);

        static u32 const l_bmdidx[2] = {BMDG_MU04, BMDG_MU05};
        for (u32 i = 0; i < 2; i++) {
            J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_bug", l_bmdidx[i]);
            JUT_ASSERT(1476, modelData != NULL);

            if (dComIfGp_addSimpleModel(modelData, fopAcM_GetRoomNo(a_this), 0) == -1) {
                OS_REPORT("1Bh[43;30m虫の集団：シンプルモデル登録失敗しました。\n1Bh[m"); // 1Bh, group of insects: Simple model registration failed.
            }

            i_this->field_0x7dad = 1;
        }

        l_roomNo = fopAcM_GetRoomNo(a_this);
        i_this->mSound.init(&a_this->current.pos, 2);
    }

    return phase;
}

AUDIO_INSTANCES;

static DUSK_CONST actor_method_class l_daE_Bug_Method = {
    (process_method_func)daE_Bug_Create,
    (process_method_func)daE_Bug_Delete,
    (process_method_func)daE_Bug_Execute,
    (process_method_func)daE_Bug_IsDelete,
    (process_method_func)daE_Bug_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_BUG = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_BUG_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(e_bug_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_BUG_e,
    /* Actor SubMtd */ &l_daE_Bug_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
