// camera.h
#pragma once
#include "esp_camera.h"
#include "sd_storage.h"

extern bool cameraReady;
extern uint16_t photoNumber;

bool setupCamera();
camera_fb_t* captureFrame();
