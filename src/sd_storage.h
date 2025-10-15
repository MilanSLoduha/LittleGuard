// sd_storage.h
#pragma once
#include "FS.h"
#include "esp_camera.h"

bool sdInit();
bool sdWritePhoto(const char *path, camera_fb_t *fb);
