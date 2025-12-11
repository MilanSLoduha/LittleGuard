// sd_storage.h
#pragma once
#include "FS.h"
#include "esp_camera.h"
#include "camera.h"
#include <Preferences.h>

extern bool sdReady;

void sdInit();
bool sdWritePhoto(const char *path, camera_fb_t *fb);
bool savePhotoNumber(int num);
uint16_t loadPhotoNumber();
bool loadWIFICredentials(String &ssid, String &password);
bool firstTime();
bool saveWIFICredentials(String &ssid, String &password);
void saveFirstTime();
bool saveCameraSetup(String &macAddress);
bool detectDoubleReset();
bool isDoubleResetWindowActive();
bool saveCameraSettingsToPrefs(const String &json);
bool loadCameraSettingsFromPrefs(String &json);
bool saveCameraSettingsToSD(const String &json);
bool loadCameraSettingsFromSD(String &json);
