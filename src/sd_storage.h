// sd_storage.h
#pragma once
#include "FS.h"
#include "esp_camera.h"
#include "camera.h"

extern bool sdReady;

bool sdInit();
bool sdWritePhoto(const char *path, camera_fb_t *fb);
bool savePhotoNumber(int num);
uint16_t loadPhotoNumber();