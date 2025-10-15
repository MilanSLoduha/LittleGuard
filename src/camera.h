// camera.h
#pragma once
#include "esp_camera.h"

bool setupCamera();
camera_fb_t* captureFrame();
