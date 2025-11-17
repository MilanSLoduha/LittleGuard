// camera.cpp
#include "camera.h"
#include "select_pins.h"
#include <Arduino.h>
#include <esp_err.h>

bool cameraReady = false;
uint16_t photoNumber;

bool setupCamera() {
	camera_config_t config;
	config.ledc_channel = LEDC_CHANNEL_0;
	config.ledc_timer = LEDC_TIMER_0;
	config.pin_d0 = Y2_GPIO_NUM;
	config.pin_d1 = Y3_GPIO_NUM;
	config.pin_d2 = Y4_GPIO_NUM;
	config.pin_d3 = Y5_GPIO_NUM;
	config.pin_d4 = Y6_GPIO_NUM;
	config.pin_d5 = Y7_GPIO_NUM;
	config.pin_d6 = Y8_GPIO_NUM;
	config.pin_d7 = Y9_GPIO_NUM;
	config.pin_xclk = XCLK_GPIO_NUM;
	config.pin_pclk = PCLK_GPIO_NUM;
	config.pin_vsync = VSYNC_GPIO_NUM;
	config.pin_href = HREF_GPIO_NUM;
	config.pin_sccb_sda = SIOD_GPIO_NUM;
	config.pin_sccb_scl = SIOC_GPIO_NUM;
	config.pin_pwdn = PWDN_GPIO_NUM;
	config.pin_reset = RESET_GPIO_NUM;
	config.xclk_freq_hz = 20000000;
	config.pixel_format = PIXFORMAT_JPEG;

	config.fb_location = CAMERA_FB_IN_PSRAM;
	config.grab_mode = CAMERA_GRAB_LATEST;

	// debug prints
	Serial.println("Camera pin configuration:");
	Serial.printf("PWDN=%d RESET=%d XCLK=%d SIOD=%d SIOC=%d\n", PWDN_GPIO_NUM, RESET_GPIO_NUM, XCLK_GPIO_NUM, SIOD_GPIO_NUM, SIOC_GPIO_NUM);

#ifdef PWR_ON_PIN
	Serial.printf("PWR_ON_PIN is defined: %d - ensuring power is stable\n", PWR_ON_PIN);
	pinMode(PWR_ON_PIN, OUTPUT);
	digitalWrite(PWR_ON_PIN, HIGH);
	delay(200);
#endif

#if defined(RESET_GPIO_NUM) && (RESET_GPIO_NUM != -1)
	Serial.printf("Pulsing RESET pin %d\n", RESET_GPIO_NUM);
	pinMode(RESET_GPIO_NUM, OUTPUT);
	digitalWrite(RESET_GPIO_NUM, LOW);
	delay(10);
	digitalWrite(RESET_GPIO_NUM, HIGH);
	delay(50);
#endif

	if (psramFound()) {
		config.frame_size = FRAMESIZE_VGA; // 640x480 namiesto UXGA 1600x1200
		config.jpeg_quality = 12;          // Mierne horšia kvalita pre menšiu veľkosť
		config.fb_count = 2;
	} else {
		config.frame_size = FRAMESIZE_QVGA; // 320x240
		config.jpeg_quality = 15;
		config.fb_count = 1;
		config.fb_location = CAMERA_FB_IN_DRAM;
	}

	esp_err_t err = esp_camera_init(&config);
	if (err != ESP_OK) {
		Serial.printf("Camera init failed with error 0x%x\n", err);
		return false;
	}
	return true;
}

camera_fb_t *captureFrame() {
	return esp_camera_fb_get();
}
