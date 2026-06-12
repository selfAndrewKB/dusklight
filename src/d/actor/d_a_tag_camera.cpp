#include "d/dolzel_rel.h" // IWYU pragma: keep

#include <cmath>
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/actor/d_a_tag_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_debug_viewer.h"
#include "d/d_s_play.h"
#if TARGET_PC
#include "dusk/coop/camera.h"
#include "dusk/coop/player_slots.h"
#endif

namespace {
bool always_true() {
    return true;
}

daAlink_c* Player;

bool is_player_riding_horse() {
    return Player->checkHorseRide() || Player->checkBoarRide();
}

bool is_player_swimming() {
    return dComIfGp_checkPlayerStatus0(0, 0x100000);
}

bool is_player_riding_canoe() {
    return Player->checkCanoeRide() != 0;
}

bool is_player_jumping_by_horse() {
    daHorse_c* phorse = dComIfGp_getHorseActor();

    return Player->checkHorseRide() && phorse->checkJump();
}

bool is_player_climbing() {
    return dComIfGp_checkPlayerStatus0(0, 0x2000008) != 0;
}

bool is_player_wear_magneboots() {
    return Player->checkMagneBootsOn();
}

bool is_player_wolf() {
    return Player->checkNowWolf();
}

bool is_player_pulling_chainblock() {
    return Player->checkChainBlockPushPull();
}

bool is_player_playing_spinner() {
    return Player->checkSpinnerRide() != 0;
}

bool is_player_sliding_slope() {
    return Player->checkLv3Slide();
}

bool is_player_on_watersurface() {
    return dComIfGp_checkCameraAttentionStatus(0, 0x800) != 0;
}

bool is_player_moving_on_spinnerrail() {
    return Player->checkSpinnerPathMove();
}

bool is_player_gliding() {
    return Player->checkCokkoGlide();
}

bool is_player_hunging_by_hookshot() {
    return dComIfGp_checkPlayerStatus1(0, 0x10000) != 0;
}

bool is_player_on_rope() {
    return Player->checkWolfRope() != 0;
}

bool is_player_using_copyrod() {
    return Player->getCopyRodCameraActor() != NULL;
}

bool is_player_hunging_wall_by_hookshot() {
    return dComIfGp_checkPlayerStatus1(0, 0x2000000) != 0;
}

bool is_player_carried_by_cargo() {
    return Player->checkCargoCarry();
}

bool is_player_playing_rodeo() {
    daHorse_c* phorse = dComIfGp_getHorseActor();

    return Player->checkHorseRide() && phorse->checkRodeoMode();
}

bool is_player_in_water() {
    return dComIfGp_checkPlayerStatus0(0, 0x100000) && !is_player_on_watersurface();
}

bool is_player_hugging_eal() {
    return Player->checkOctaIealHang();
}

bool check_tag_area(daTag_Cam_c* tag, cXyz pos) {
    bool hit = false;

    if (tag->getAreaNoChk()) {
        hit = true;
    } else if (tag->getAreaType() == 0) {
        if (tag->home.angle.y != 0) {
            mDoMtx_stack_c::transS(tag->current.pos);
            mDoMtx_stack_c::YrotM(-tag->home.angle.y);

            cXyz offset = pos - tag->current.pos;
            mDoMtx_stack_c::multVec(&offset, &pos);
        }

        if (tag->mBoundsLo.x <= pos.x && pos.x <= tag->mBoundsHi.x &&
            tag->mBoundsLo.y <= pos.y && pos.y <= tag->mBoundsHi.y &&
            tag->mBoundsLo.z <= pos.z && pos.z <= tag->mBoundsHi.z)
        {
            hit = true;
        }
    } else {
        f32 temp_f31 = tag->current.pos.x - pos.x;
        f32 temp_f30 = tag->current.pos.z - pos.z;
        f32 sq_dist = std::sqrt(temp_f31 * temp_f31 + temp_f30 * temp_f30);
        if (sq_dist < tag->scale.x && tag->mBoundsLo.y <= pos.y && pos.y <= tag->mBoundsHi.y) {
            hit = true;
        }
    }

    return hit;
}

bool should_apply_tag_camera(daTag_Cam_c* tag, u16* priority) {
    *priority = tag->getPrio();
    u8 condition = tag->getCondition();
    bool set_camera = tag->mCheckFunc();

    if (condition == 0xFF) {
#if PLATFORM_SHIELD
        *priority |= (u16)0x8000;
#else
        *priority |= 0x8000;
#endif
    } else if (condition == 0xFA) {
        if (dCam_getBody()->CheckFlag(0x8000000)) {
            set_camera = true;
        }
    }

    return set_camera;
}
}  // namespace

u8 daTag_Cam_c::getSwType() {
    return fopAcM_GetParam(this) & 0xF;
}

u8 daTag_Cam_c::getPrio() {
    return (fopAcM_GetParam(this) & 0xF0) >> 4;
}

u8 daTag_Cam_c::getSwBit() {
    return (fopAcM_GetParam(this) & 0xFF00) >> 8;
}

u8 daTag_Cam_c::getCondition() {
    return (fopAcM_GetParam(this) & 0xFF0000) >> 16;
}

u8 daTag_Cam_c::getCameraId() {
    return (fopAcM_GetParam(this) & 0xFF000000) >> 24;
}

u8 daTag_Cam_c::getRailID() {
    return home.angle.z & 0xFF;
}

u16 daTag_Cam_c::getAreaType() {
    return home.angle.z & 0x100;
}

u16 daTag_Cam_c::getAreaNoChk() {
    return home.angle.z & 0x200;
}

#if PLATFORM_GCN
static u8 const lit_3874[12] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#endif

int daTag_Cam_c::create() {
    fopAcM_ct(this, daTag_Cam_c);

    if (getAreaType() == 0) {
        scale.x *= 500.0f;
        scale.y *= 1000.0f;
        scale.z *= 500.0f;

        mBoundsLo.set(current.pos.x - scale.x, current.pos.y, current.pos.z - scale.z);
        mBoundsHi.set(current.pos.x + scale.x, current.pos.y + scale.y, current.pos.z + scale.z);
    } else {
        scale.x *= 1000.0f;
        scale.y *= 1000.0f;
        scale.z *= 0.0f;

        mBoundsLo.set(0.0f, current.pos.y, 0.0f);
        mBoundsHi.set(0.0f, current.pos.y + scale.y, 0.0f);
    }

    Player = daAlink_getAlinkActorClass();

    bool (*check_func[])() = {
        is_player_riding_horse,
        is_player_swimming,
        is_player_riding_canoe,
        always_true,
        is_player_jumping_by_horse,
        is_player_climbing,
        is_player_wear_magneboots,
        is_player_wolf,
        is_player_pulling_chainblock,
        is_player_playing_spinner,
        is_player_sliding_slope,
        is_player_on_watersurface,
        is_player_moving_on_spinnerrail,
        is_player_gliding,
        is_player_hunging_by_hookshot,
        is_player_on_rope,
        is_player_using_copyrod,
        is_player_hunging_wall_by_hookshot,
        is_player_carried_by_cargo,
        is_player_playing_rodeo,
        is_player_in_water,
        is_player_hugging_eal,
    };

    int var_r27 = 22;

    u8 func_index = getCondition();
    if ((int)func_index > var_r27) {
        func_index = 3;
    }

    mCheckFunc = check_func[func_index];
    return cPhs_COMPLEATE_e;
}

int daTag_Cam_c::execute() {
    if (fopAcM_GetRoomNo(this) != dComIfGp_roomControl_getStayNo()) {
        return 0;
    }

    u8 sw_type = getSwType();
    u8 sw_bit = getSwBit();
    bool var_r29 = false;
    bool do_area_check = false;

    if (sw_bit != 0xFF) {
        bool is_switch_on = dComIfGs_isSwitch(sw_bit, fopAcM_GetRoomNo(this)) != 0;
        if ((sw_type != 0 && is_switch_on) || (sw_type == 0 && !is_switch_on)) {
            do_area_check = true;
        }
    } else {
        do_area_check = true;
    }

    if (do_area_check) {
        cXyz pos(dComIfGp_getLinkPlayer()->current.pos);
        if (dComIfGp_checkPlayerStatus0(0, 8)) {
            pos = dComIfGp_getLinkPlayer()->attention_info.position;
            pos.y -= 80.0f;
        }

        var_r29 = check_tag_area(this, pos);
    }

    if (var_r29) {
        u16 priority;
        if (should_apply_tag_camera(this, &priority)) {
            u8 cam_id = getCameraId();
            u8 rail_id = getRailID();
            dCam_getBody()->SetTagData(this, cam_id, priority, rail_id);
        }
    }

#if TARGET_PC
    if (dusk::coop::camera::isSplitScreenEnabled() && dusk::coop::camera::isSecondaryCameraReady()) {
        fopAc_ac_c* secondary = dusk::coop::getPlayer(dusk::coop::PlayerSlot::Secondary);
        camera_process_class* camera = dComIfGp_getCamera(dusk::coop::camera::kSecondaryCameraId);
        if (secondary != NULL && camera != NULL && fopAcM_GetRoomNo(this) == fopAcM_GetRoomNo(secondary) &&
            check_tag_area(this, secondary->current.pos))
        {
            daAlink_c* previous_player = Player;
            // Co-op: native camera tags are P1-global, so split screen evaluates the same tag for P2.
            Player = static_cast<daAlink_c*>(secondary);
            u16 priority;
            if (should_apply_tag_camera(this, &priority)) {
                u8 cam_id = getCameraId();
                u8 rail_id = getRailID();
                camera->mCamera.SetTagData(this, cam_id, priority, rail_id);
            }
            Player = previous_player;
        }
    }
#endif

    return 1;
}

static int daTag_Cam_Draw(daTag_Cam_c* i_this) {
#if DEBUG
    return i_this->draw();
#else
    return 1;
#endif
}

int daTag_Cam_c::draw() {
#if DEBUG
    if (g_envHIO.mOther.mDisplayTransparentCyl != 0) {
        if (getAreaType() == 0) {
            cXyz sp3C = (mBoundsHi - mBoundsLo) * 0.5f;
            cXyz sp30 = mBoundsLo + sp3C;
            csXyz sp10(0, 0, 0);
            dDbVw_drawCubeXlu(sp30, sp3C, sp10, (GXColor){0xc0, 0xff, 0x78, 0xa0});
        } else {
            cXyz cStack_3c = current.pos;
            dDbVw_drawCylinderXlu(cStack_3c, scale.x, scale.y, (GXColor){0xc0, 0xff, 0x78, 0xa0},
                                  1);
        }
    }
#endif
    return 1;
}

static int daTag_Cam_Execute(daTag_Cam_c* i_this) {
    i_this->execute();
    return 1;
}

static int daTag_Cam_IsDelete(daTag_Cam_c* i_this) {
    return 1;
}

static int daTag_Cam_Delete(daTag_Cam_c* i_this) {
    int id = fopAcM_GetID(i_this);
    i_this->~daTag_Cam_c();
    return 1;
}

static int daTag_Cam_Create(fopAc_ac_c* i_this) {
    daTag_Cam_c* cam = (daTag_Cam_c*)i_this;
    int id = fopAcM_GetID(i_this);
    int result = cam->create();
    return result;
}

static DUSK_CONST actor_method_class l_daTag_Cam_Method = {
    (process_method_func)daTag_Cam_Create,  (process_method_func)daTag_Cam_Delete,
    (process_method_func)daTag_Cam_Execute, (process_method_func)daTag_Cam_IsDelete,
    (process_method_func)daTag_Cam_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_TAG_CAMERA = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_TAG_CAMERA_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daTag_Cam_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_TAG_CAMERA_e,
    /* Actor SubMtd */ &l_daTag_Cam_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_6_e,
};
