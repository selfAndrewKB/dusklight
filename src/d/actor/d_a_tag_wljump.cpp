#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_tag_wljump.h"
#include "d/d_path.h"
#include "f_pc/f_pc_name.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_midna.h"

#if TARGET_PC
#include "d/actor/d_a_alink.h"
#include "dusk/coop/midna_owner.h"
#include "dusk/coop/player_attention.h"
#include "dusk/coop/player_slots.h"
#endif

#if TARGET_PC
namespace {
daTagWljump_c::CoopTraversalState* getCoopTraversalState(daTagWljump_c* tag,
                                                         const daAlink_c* player) {
    const dusk::coop::PlayerSlot slot = dusk::coop::getSlotForActor(player);
    const int slotIndex = static_cast<int>(slot);
    if (slotIndex < static_cast<int>(dusk::coop::PlayerSlot::Primary) ||
        slotIndex >= dusk::coop::kPlayerSlotCount)
    {
        return NULL;
    }

    return &tag->mCoopTraversal[slotIndex];
}

const daTagWljump_c::CoopTraversalState* getCoopTraversalState(
    const daTagWljump_c* tag, const daAlink_c* player)
{
    return getCoopTraversalState(const_cast<daTagWljump_c*>(tag), player);
}

void updateCoopPlayers(daTagWljump_c* tag) {
    for (int i = 0; i < dusk::coop::kPlayerSlotCount; i++) {
        daAlink_c* player = static_cast<daAlink_c*>(
            dusk::coop::getPlayer(static_cast<dusk::coop::PlayerSlot>(i)));
        if (player != NULL) {
            tag->updateCoopPlayerState(player);
        }
    }
}

bool isPrimaryPlayer(const daAlink_c* player) {
    return dusk::coop::getSlotForActor(player) == dusk::coop::PlayerSlot::Primary;
}

// Co-op: P1 remains canonical except while its no-message Midna approach is active.
bool usesCoopTraversalState(const daAlink_c* player,
                            const daTagWljump_c::CoopTraversalState* state) {
    return state != NULL &&
           (!isPrimaryPlayer(player) ||
            state->approachPhase != daTagWljump_c::CoopApproachPhase::Idle);
}

bool setTraversalPoint(daTagWljump_c* tag, daTagWljump_c::CoopTraversalState* state,
                       int pointIndex) {
    if (tag->field_0x5c4 == NULL || pointIndex < 0 || pointIndex >= tag->field_0x5c4->m_num) {
        return false;
    }

    dPnt* point = &tag->field_0x5c4->m_points[pointIndex];
    state->lockPoint = pointIndex;
    state->lockPos = point->m_position;
    state->attentionPos = state->lockPos;
    state->attentionPos.y += 220.0f;
    state->landArea = point->mArg0 * 10.0f;
    state->notSlide = point->mArg2 == 1;
    return true;
}

void installPrimaryReadyState(daTagWljump_c* tag, const daAlink_c* player,
                              const daTagWljump_c::CoopTraversalState* state) {
    if (!isPrimaryPlayer(player) || state == NULL || state->talkPoint < 0) {
        return;
    }

    // Co-op: no-message P1 staging still commits the same canonical fields the
    // vanilla talk scheduler would have written after Midna reached the point.
    tag->field_0x568 = state->talkPoint;
    tag->field_0x572 = 1;
    tag->field_0x574 = 0;
    tag->eyePos = state->lockPos;
    tag->attention_info.position = state->attentionPos;
    tag->mLandArea = state->landArea;
    tag->shape_angle.z = state->notSlide;
}
}  // namespace

const cXyz* daTagWljump_c::getLockPos(const daAlink_c* player) const {
    const CoopTraversalState* state = getCoopTraversalState(this, player);
    if (!usesCoopTraversalState(player, state)) {
        return getLockPos();
    }

    return state->owner == player && state->ownerId == fopAcM_GetID(player) &&
                   state->lockPoint >= 0
               ? &state->lockPos
               : NULL;
}

f32 daTagWljump_c::getLandArea(const daAlink_c* player) const {
    const CoopTraversalState* state = getCoopTraversalState(this, player);
    if (!usesCoopTraversalState(player, state)) {
        return getLandArea();
    }

    return state->owner == player && state->ownerId == fopAcM_GetID(player)
               ? state->landArea
               : 0.0f;
}

void daTagWljump_c::onNextCheckFlg(const daAlink_c* player) {
    CoopTraversalState* state = getCoopTraversalState(this, player);
    if (state != NULL && !isPrimaryPlayer(player)) {
        if (state->owner == player && state->ownerId == fopAcM_GetID(player)) {
            state->nextCheck = true;
        }
    } else {
        onNextCheckFlg();
    }
}

s16 daTagWljump_c::getNotSlideFlg(const daAlink_c* player) const {
    const CoopTraversalState* state = getCoopTraversalState(this, player);
    if (!usesCoopTraversalState(player, state)) {
        return getNotSlideFlg();
    }

    return state->owner == player && state->ownerId == fopAcM_GetID(player)
               ? state->notSlide
               : 0;
}

u32 daTagWljump_c::getAttentionFlags(const daAlink_c* player) const {
    const CoopTraversalState* state = getCoopTraversalState(this, player);
    if (!usesCoopTraversalState(player, state)) {
        return attention_info.flags;
    }

    return state->owner == player && state->ownerId == fopAcM_GetID(player)
               ? state->attentionFlags
               : 0;
}

const cXyz& daTagWljump_c::getAttentionPosition(const daAlink_c* player) const {
    const CoopTraversalState* state = getCoopTraversalState(this, player);
    if (usesCoopTraversalState(player, state) && state->owner == player &&
        state->ownerId == fopAcM_GetID(player))
    {
        return state->attentionPos;
    }

    return attention_info.position;
}

bool daTagWljump_c::requiresTutorialMessage() {
    return shape_angle.x != 0 &&
           (field_0x571 == 0xff || !fopAcM_isSwitch(this, field_0x571));
}

bool daTagWljump_c::beginCoopTraversal(daAlink_c* player) {
    CoopTraversalState* state = getCoopTraversalState(this, player);
    if (state == NULL) {
        return false;
    }

    const fpc_ProcID playerId = fopAcM_GetID(player);
    if (state->owner != player || state->ownerId != playerId) {
        *state = CoopTraversalState{};
        state->owner = player;
        state->ownerId = playerId;
    }

    if (isPrimaryPlayer(player)) {
        state->currentPoint = field_0x56a;
        state->talkPoint = field_0x570;
        if (!setTraversalPoint(this, state, state->talkPoint)) {
            return false;
        }
    } else if (state->talkPoint < 0) {
        return false;
    }

    // Co-op: ordinary wolf-jump travel is a per-slot Midna ability, not a talk event.
    state->approachPhase = CoopApproachPhase::Traveling;
    state->ready = false;
    state->lockPoint = state->talkPoint;
    state->attentionFlags = 0;
    return true;
}

void daTagWljump_c::releaseCoopApproach(daAlink_c* player) {
    CoopTraversalState* state = getCoopTraversalState(this, player);
    if (state == NULL || state->owner != player || state->ownerId != fopAcM_GetID(player)) {
        return;
    }

    // Co-op: ALINK's actor keep owns continuation after proc init has copied
    // the point data, so Midna's approach service can end at this exact handoff.
    state->approachPhase = CoopApproachPhase::Idle;
    dusk::coop::midna_owner::endAbilityService(player, this);
}

u8 daTagWljump_c::getCoopApproachPhase(const daAlink_c* player) const {
    const CoopTraversalState* state = getCoopTraversalState(this, player);
    return state != NULL && state->owner == player && state->ownerId == fopAcM_GetID(player)
               ? static_cast<u8>(state->approachPhase)
               : static_cast<u8>(CoopApproachPhase::Idle);
}

bool daTagWljump_c::isCoopTraversalReady(const daAlink_c* player) const {
    const CoopTraversalState* state = getCoopTraversalState(this, player);
    if (!usesCoopTraversalState(player, state)) {
        return field_0x572 != 0;
    }
    return state != NULL && state->owner == player && state->ownerId == fopAcM_GetID(player) &&
           state->ready;
}

void daTagWljump_c::updateCoopPlayerState(daAlink_c* player) {
    CoopTraversalState* state = getCoopTraversalState(this, player);
    if (state == NULL) {
        return;
    }

    const fpc_ProcID playerId = fopAcM_GetID(player);
    if (state->owner != player || state->ownerId != playerId) {
        // Co-op: tag traversal is scene-actor state and must not survive a slot actor replacement.
        *state = CoopTraversalState{};
        state->owner = player;
        state->ownerId = playerId;
    }

    if (isPrimaryPlayer(player) && state->approachPhase == CoopApproachPhase::Idle) {
        return;
    }

    state->attentionFlags = 0;
    if (state->approachPhase != CoopApproachPhase::Idle &&
        dusk::coop::midna_owner::abilityPartnerForPlayer(player) != this)
    {
        // Co-op: a newer slot-local ability supersedes this tag without leaving
        // its old Midna destination or Jump target exposed.
        state->approachPhase = CoopApproachPhase::Idle;
        state->ready = false;
        state->lockPoint = -1;
        return;
    }

    daMidna_c* midna = dusk::coop::midna_owner::getMidnaForPlayer(player);
    if (midna == NULL || !player->checkWolf() || !daPy_py_c::checkFirstMidnaDemo() ||
        midna->checkMidnaTired())
    {
        if (state->approachPhase != CoopApproachPhase::Idle) {
            state->approachPhase = CoopApproachPhase::Idle;
            dusk::coop::midna_owner::endAbilityService(player, this);
        }
        state->ready = false;
        state->lockPoint = -1;
        return;
    }

    if (state->approachPhase == CoopApproachPhase::Traveling) {
        state->lockPoint = state->talkPoint;
        const cXyz* talkPos = state->lockPoint >= 0 ? &state->lockPos : NULL;
        if (!midna->checkShadowModeTalkWait() && talkPos != NULL &&
            midna->current.pos.abs(*talkPos) < 5.0f)
        {
            // Co-op: reaching the point exposes the native lock/Jump handoff;
            // retain the ability partner until ALINK actually enters the proc.
            state->approachPhase = CoopApproachPhase::Stationed;
            state->ready = true;
            state->noPosFrames = 0;
            field_0x573 = 0;
            if (field_0x571 != 0xff) {
                fopAcM_onSwitch(this, field_0x571);
            }
            installPrimaryReadyState(this, player, state);
        }
        return;
    }

    if (state->approachPhase == CoopApproachPhase::Stationed) {
        state->lockPoint = state->talkPoint;
        state->attentionFlags = fopAc_AttnFlag_ETC_e | fopAc_AttnFlag_LOCK_e;
        return;
    }

    if (eventInfo.checkCommandTalk() && dusk::coop::midna_owner::isServiceActive() &&
        dusk::coop::midna_owner::currentPlayer() == player)
    {
        // Co-op: the accepted tag conversation owns this slot's retained approach point.
        return;
    }

    // Co-op: readiness follows this slot's Midna position lifecycle. The
    // singleton event order flag belongs only to real tutorial-message flows.
    if (!midna->checkWolfNoPos()) {
        state->noPosFrames++;
        if (state->noPosFrames >= 5) {
            state->ready = false;
        }
    } else {
        state->noPosFrames = 0;
    }

    if (field_0x571 != 0xff && fopAcM_isSwitch(this, field_0x571)) {
        field_0x56c = 0;
    }

    if (field_0x56c == 0 && !state->ready && field_0x571 != 0xff &&
        !fopAcM_isSwitch(this, field_0x571))
    {
        state->lockPoint = -1;
        return;
    }

    dPnt* point = field_0x5c4->m_points;
    if (!player->checkWolfTagLockJumpLand()) {
        if (!player->checkWolfTagLockJump()) {
            int pointIndex;
            for (pointIndex = 0; pointIndex < field_0x5c4->m_num; pointIndex++, point++) {
                if (player->current.pos.abs2(point->m_position) <
                    point->mArg1 * point->mArg1 * 10.0f * 10.0f)
                {
                    state->currentPoint = pointIndex;
                    if (pointIndex == 0) {
                        state->lockPoint = 1;
                    } else if (pointIndex == field_0x5c4->m_num - 1) {
                        state->lockPoint = pointIndex - 1;
                    } else {
                        state->lockPoint = pointIndex + 1;
                    }
                    break;
                }
            }

            if (pointIndex == field_0x5c4->m_num) {
                state->lockPoint = -1;
            }
        } else if (state->nextCheck) {
            state->nextCheck = false;
            if (state->currentPoint < state->lockPoint) {
                state->lockPoint++;
                if (field_0x5c4->m_num == state->lockPoint) {
                    state->lockPoint = -1;
                }
            } else {
                state->lockPoint--;
            }
        }
    }

    if (state->lockPoint < 0) {
        state->ready = false;
        return;
    }

    setTraversalPoint(this, state, state->lockPoint);

    if (!state->ready) {
        if (!dComIfGp_event_runCheck()) {
            eventInfo.onCondition(dEvtCnd_CANTALK_e);
            if (!player->checkPlayerFly() && player->eventInfo.chkCondition(dEvtCnd_CANTALK_e)) {
                // Co-op: each Link's Z-hint queue receives the tag selected from its own position.
                dusk::coop::player_attention::requestZHintForPlayer(player, this, 0x1ff);
                if (field_0x56e == 0) {
                    field_0x56e = 1;
                    if (field_0x56d == 0) {
                        mDoAud_seStart(Z2SE_NAVI_CALLVOICE, 0, 0, 0);
                    }
                    field_0x56d = 60;
                }
            }
        }

        state->talkPoint = state->lockPoint;
        state->lockPoint = -1;
    } else {
        state->attentionFlags = fopAc_AttnFlag_ETC_e | fopAc_AttnFlag_LOCK_e;
    }
}
#endif

int daTagWljump_c::create() {
    fopAcM_ct(this, daTagWljump_c);

    field_0x571 = (fopAcM_GetParam(this) >> 8) & 0xFF;

    int path_no = fopAcM_GetParam(this) & 0xFF;
    if (path_no == 0xFF) {
        return 5;
    }

    field_0x5c4 = dPath_GetRoomPath(path_no, fopAcM_GetRoomNo(this));
    if (field_0x5c4 == NULL || field_0x5c4->m_num < 2) {
        return cPhs_ERROR_e;
    }

    attention_info.distances[fopAc_attn_LOCK_e] = 50;
    attention_info.distances[fopAc_attn_ETC_e] = 50;

    shape_angle.z = 0;
    field_0x568 = -1;

    field_0x56c = (fopAcM_GetParam(this) >> 16) & 0xF;
    if (field_0x56c != 1) {
        field_0x56c = 0;
    }

    if (field_0x56c == 0 && field_0x571 != 0xFF) {
        if (!fopAcM_isSwitch(this, field_0x571)) {
            field_0x573 = 1;
        }
    }

    return cPhs_COMPLEATE_e;
}

static int daTagWljump_Create(fopAc_ac_c* i_this) {
    daTagWljump_c* a_this = (daTagWljump_c*)i_this;
    fpc_ProcID id = fopAcM_GetID(i_this);

    return a_this->create();
}

daTagWljump_c::~daTagWljump_c() {
#if TARGET_PC
    // Co-op: a shared tag can be retained by several slot-local Midna abilities.
    dusk::coop::midna_owner::endAbilityServicesForPartner(this);
#endif
}

static int daTagWljump_Delete(daTagWljump_c* i_this) {
    fpc_ProcID id = fopAcM_GetID(i_this);

    i_this->~daTagWljump_c();
    return 1;
}

int daTagWljump_c::execute() {
    attention_info.flags = 0;

    if (field_0x56d) {
        field_0x56d--;
    }

    dPnt* point_p;

    daPy_py_c* player = daPy_getLinkPlayerActorClass();
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (midna == NULL) {
#if TARGET_PC
        updateCoopPlayers(this);
#endif
        return 1;
    }

    if (eventInfo.checkCommandTalk()) {
#if TARGET_PC
        daAlink_c* talkPlayer = dusk::coop::midna_owner::isServiceActive()
                                    ? dusk::coop::midna_owner::currentPlayer()
                                    : static_cast<daAlink_c*>(player);
        daMidna_c* talkMidna = talkPlayer != NULL
                                   ? dusk::coop::midna_owner::getMidnaForPlayer(talkPlayer)
                                   : midna;
        if (talkMidna == NULL) {
            talkMidna = midna;
        }
#else
        daMidna_c* talkMidna = midna;
#endif
        BOOL spC = TRUE;
        if (!talkMidna->checkShadowModeTalkWait()) {
            if (shape_angle.x != 0 && (field_0x571 == 0xff || !fopAcM_isSwitch(this, field_0x571))) {
                if (field_0x56f == 0) {
                    mMsgFlow.init(this, (u16)shape_angle.x, 0, NULL);
                    field_0x56f = 1;
                    mDoAud_seStart(Z2SE_NAVI_TALK_START, NULL, 0, 0);
                } else {
                    if (mMsgFlow.doFlow(this, NULL, 0)) {
                        mDoAud_seStart(Z2SE_NAVI_TALK_END, NULL, 0, 0);
                        shape_angle.x = 0;
                    }
                }
            } else {
#if TARGET_PC
                CoopTraversalState* talkState = getCoopTraversalState(this, talkPlayer);
                // Co-op: P1 tutorial messages continue through canonical tag fields.
                if (!usesCoopTraversalState(talkPlayer, talkState)) {
                    talkState = NULL;
                }
                const cXyz* talkPos;
                if (talkState != NULL) {
                    talkState->lockPoint = talkState->talkPoint;
                    talkPos = talkState->lockPoint >= 0 ? &talkState->lockPos : NULL;
                } else {
                    field_0x568 = field_0x570;
                    talkPos = &eyePos;
                }
#else
                field_0x568 = field_0x570;
                const cXyz* talkPos = &eyePos;
#endif
                if (talkPos != NULL && talkMidna->current.pos.abs(*talkPos) < 5.0f) {
                    spC = FALSE;
                }
            } 
        }

        if (spC) {
#if TARGET_PC
            updateCoopPlayers(this);
#endif
            return 1;
        }

        field_0x56f = 0;
        dComIfGp_event_reset();
        field_0x56c = 0;
#if TARGET_PC
        CoopTraversalState* talkState = getCoopTraversalState(this, talkPlayer);
        if (!usesCoopTraversalState(talkPlayer, talkState)) {
            talkState = NULL;
        }
        if (talkState != NULL) {
            // Co-op: only the companion that reached this point makes its Link jump-ready.
            talkState->approachPhase = CoopApproachPhase::Idle;
            talkState->ready = true;
            talkState->noPosFrames = 0;
            field_0x573 = 0;
            installPrimaryReadyState(this, talkPlayer, talkState);
        } else {
            field_0x572 = 1;
        }
#else
        field_0x572 = 1;
#endif
        if (field_0x571 != 0xff) {
            fopAcM_onSwitch(this, field_0x571);
        }
    } else if (!dComIfGp_getEvent()->isOrderOK()) {
        field_0x572 = 0;
    } else {
        if (!midna->checkWolfNoPos()) {
            field_0x574++;
            if (field_0x574 >= 5) {
                field_0x572 = 0;
            }
        } else {
            field_0x574 = 0;
        }
    }

    if (!player->checkNowWolf() || !daPy_py_c::checkFirstMidnaDemo() || midna->checkMidnaTired()) {
#if TARGET_PC
        updateCoopPlayers(this);
#endif
        return 1;
    } 

    if (field_0x571 != 0xff && fopAcM_isSwitch(this, field_0x571)) {
        field_0x56c = 0;
        if (field_0x573) {
            field_0x573 = 0;
            field_0x572 = 1;
            field_0x574 = 0;
        }
    }

    if (field_0x56c != 0 || field_0x572 != 0 || field_0x571 == 0xff || fopAcM_isSwitch(this, field_0x571)) {
        int var_r28;
        point_p = field_0x5c4->m_points;

        if (!player->checkWolfTagLockJumpLand()) {
            if (!player->checkWolfTagLockJump()) {
                for (var_r28 = 0; var_r28 < field_0x5c4->m_num; var_r28++, point_p++) {
                    if (player->current.pos.abs2(point_p->m_position) < point_p->mArg1 * point_p->mArg1 * 10.0f * 10.0f) {
                        field_0x56a = var_r28;
                        if (var_r28 == 0) {
                            field_0x568 = 1;
                        } else if (var_r28 == field_0x5c4->m_num - 1) {
                            field_0x568 = var_r28 - 1;
                        } else {
                            field_0x568 = var_r28 + 1;
                        }
                        break;
                    }
                }

                if (var_r28 == field_0x5c4->m_num) {
                    field_0x568 = -1;
                }
            } else if (mNextCheckFlg) {
                mNextCheckFlg = 0;
                if (field_0x56a < field_0x568) {
                    field_0x568++;
                    if (field_0x5c4->m_num == field_0x568) {
                        field_0x568 = 0xff;
                    }
                } else {
                    field_0x568--;
                }
            }
        }

        if (field_0x568 >= 0) {
            point_p = &field_0x5c4->m_points[field_0x568];

            eyePos.set(point_p->m_position.x, point_p->m_position.y, point_p->m_position.z);
            attention_info.position = eyePos;
            attention_info.position.y += 220.0f;

            mLandArea = point_p->mArg0 * 10.0f;

            if (point_p->mArg2 == 1) {
                shape_angle.z = 1;
            } else {
                shape_angle.z = 0;
            }

            if (field_0x572 == 0) {
                if (!dComIfGp_event_runCheck()) {
                    eventInfo.onCondition(dEvtCnd_CANTALK_e);
                    if (!player->checkPlayerFly() && player->eventInfo.chkCondition(dEvtCnd_CANTALK_e)) {
                        dComIfGp_att_ZHintRequest(this, 0x1FF);

                        if (field_0x56e == 0)  {
                            field_0x56e = 1;
                            if (field_0x56d == 0) {
                                mDoAud_seStart(Z2SE_NAVI_CALLVOICE, 0, 0, 0);
                            }
                            field_0x56d = 60;
                        }
                    }
                }

                field_0x570 = field_0x568;
                field_0x568 = -1;
            } else {
                attention_info.flags |= fopAc_AttnFlag_ETC_e | fopAc_AttnFlag_LOCK_e;
            }
        } else {
            field_0x572 = 0;
        }
    } else {
        field_0x572 = 0;
        field_0x568 = -1;
    }

    current.pos = attention_info.position;

    if (!eventInfo.chkCondition(dEvtCnd_CANTALK_e)) {
        field_0x56e = 0;
    }

#if TARGET_PC
    updateCoopPlayers(this);
#endif
    
    return 1;
}

static int daTagWljump_Execute(daTagWljump_c* i_this) {
    return i_this->execute();
}

int daTagWljump_c::draw() {
    return 1;
}

static int daTagWljump_Draw(daTagWljump_c* i_this) {
    return i_this->draw();
}

static DUSK_CONST actor_method_class l_daTagWljump_Method = {
    (process_method_func)daTagWljump_Create,
    (process_method_func)daTagWljump_Delete,
    (process_method_func)daTagWljump_Execute,
    NULL,
    (process_method_func)daTagWljump_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_Tag_Wljump = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Tag_Wljump_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daTagWljump_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Tag_Wljump_e,
    /* Actor SubMtd */ &l_daTagWljump_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    /* Group        */ fopAc_ENV_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
