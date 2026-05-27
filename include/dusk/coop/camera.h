#pragma once

#include "dolphin/types.h"

class camera_class;
class cXyz;
class dDlst_window_c;
class fopAc_ac_c;

namespace dusk::coop::camera {

enum class SplitScreenLayout : u8 {
    Vertical = 0,
    Horizontal = 1,
};

constexpr int kPrimaryCameraId = 0;
constexpr int kSecondaryCameraId = 1;
constexpr int kSecondaryWindowId = 1;
constexpr int kSecondaryPlayerId = 1;

bool isSplitScreenEnabled();
void setSplitScreenEnabled(bool enabled);
SplitScreenLayout getSplitScreenLayout();
void setSplitScreenLayout(SplitScreenLayout layout);

void resetSplitScreenCameraState();
bool ensureSecondaryCamera();
void syncSecondaryPlayerAssignment();
void refreshWindowLayout();
void refreshWindowLayoutForCamera(int cameraId);

bool isExtensionIndex(int idx);
int getEffectiveWindowNum(int baseWindowNum);

dDlst_window_c* getWindow(int idx);
void setWindow(int idx, f32 x, f32 y, f32 width, f32 height, f32 nearZ, f32 farZ, int camId,
               int mode);

camera_class* getCamera(int idx);
void setCamera(int idx, camera_class* camera);
int getCameraWinID(int idx);
int getCameraPlayer1ID(int idx);
int getCameraPlayer2ID(int idx);
u32 getCameraAttentionStatus(int idx);
BOOL checkCameraAttentionStatus(int idx, u32 flag);
void setCameraAttentionStatus(int idx, u32 flag);
void onCameraAttentionStatus(int idx, u32 flag);
void offCameraAttentionStatus(int idx, u32 flag);
void setCameraInfo(int idx, camera_class* camera, int winId, int player1Id, int player2Id);
f32 getCameraZoomScale(int idx);
void setCameraZoomScale(int idx, f32 scale);
f32 getCameraZoomForcus(int idx);
void setCameraZoomForcus(int idx, f32 focus);
const char* getCameraParamFileName(int idx);
void setCameraParamFileName(int idx, char* name);
void saveCameraPosition(int idx, cXyz* pos, cXyz* target, f32 fovy, s16 bank);
void loadCameraPosition(int idx, cXyz* pos, cXyz* target, f32* fovy, s16* bank);
fopAc_ac_c* getPlayer(int idx);
void setPlayer(int idx, fopAc_ac_c* player);
int getPlayerCameraID(int idx);
void setPlayerInfo(int idx, fopAc_ac_c* player, int cameraId);

f32 getWindowAspect(int cameraId);
bool isSecondaryCameraReady();
bool isSecondaryCameraRequested();

}  // namespace dusk::coop::camera
