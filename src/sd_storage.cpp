// sd_storage.cpp
#include "sd_storage.h"
#include "SD.h"
#include "connected_devices.h"
#include "select_pins.h"
#include <Arduino.h>
#include <ESP.h>
#include <esp_camera.h>
#include <esp_timer.h>

bool sdReady = false;
Preferences prefs;
static uint16_t videoNumber = 1;

void sdInit() {
	SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
	if (!SD.begin(SD_CS_PIN, SPI)) {
		sdReady = false;
		Serial.println("SD.begin() failed");
		return;
	}
	uint8_t cardType = SD.cardType();
	if (cardType == CARD_NONE) {
		sdReady = false;
		Serial.println("No SD card attached");
		return;
	}

	sdReady = true;
	photoNumber = loadPhotoNumber();
	videoNumber = loadVideoNumber();
}

bool sdWritePhoto(const char *path, camera_fb_t *fb) {
	if(!sdReady) {
		sdInit();
		if(!sdReady) {
			Serial.println("kamo ono to neotvara sd kartu");
		}
	} 
	File file = SD.open(path, FILE_WRITE);
	if (!file) {
		Serial.println("Failed to open file for writing (SD)");
		return false;
	}
	file.write(fb->buf, fb->len);
	file.close();
	Serial.printf("Saved %u bytes to %s\n", (unsigned)fb->len, path);
	return true;
}

bool savePhotoNumber(int num) {
	prefs.begin("camera", false);
	prefs.putInt("num", num);
	prefs.end();
	return true;
}

uint16_t loadPhotoNumber() {
	prefs.begin("camera", true);
	int num = prefs.getInt("num", 1);
	prefs.end();
	if (num < 1) num = 1;
	return num;
}

bool saveVideoNumber(int num) {
	prefs.begin("video", false);
	prefs.putInt("num", num);
	prefs.end();
	return true;
}

uint16_t loadVideoNumber() {
	prefs.begin("video", true);
	int num = prefs.getInt("num", 1);
	prefs.end();
	return num;
}

bool loadWIFICredentials(String &ssid, String &password) {
	prefs.begin("wifi", true);
	ssid = prefs.getString("ssid", "");
	password = prefs.getString("password", "");
	bool hasCredentials = ssid.length() > 0;
	prefs.end();
	return hasCredentials;
}

bool firstTime() {
	prefs.begin("wifi", true);
	bool firstBoot = prefs.getBool("first", true);
	prefs.end();
	return firstBoot;
}

bool saveWIFICredentials(String &ssid, String &password) {
	prefs.begin("wifi", false);
	prefs.putString("ssid", ssid);
	prefs.putString("password", password);
	prefs.putBool("first", false);
	prefs.end();
	return true;
}

void saveFirstTime() {
	prefs.begin("wifi", false);
	prefs.putBool("first", false);
	prefs.end();
}

bool saveCameraSetup(String &macAddress) {
	if(!sdReady) {
		sdInit();
		if(!sdReady) {
			Serial.println("kamo ono to neotvara sd kartu");
		}
	} 
	File file = SD.open("/camSet.txt", FILE_WRITE);
	if (!file) {
		Serial.println("Failed to open camSet.txt for writing");
		return false;
	}

	sensor_t *s = esp_camera_sensor_get();
	if (s) {
		file.println(String(s->status.framesize));
		file.println(String(s->status.quality));
		file.println(String(s->status.brightness));
		file.println(String(s->status.contrast));
		file.println(String(s->status.saturation));
		file.println(String(s->status.special_effect));
		file.println(String(s->status.wb_mode));
		file.println(String(s->status.awb));
		file.println(String(s->status.awb_gain));
		file.println(String(s->status.aec));
		file.println(String(s->status.aec2));
		file.println(String(s->status.ae_level));
		file.println(String(s->status.aec_value));
		file.println(String(s->status.agc));
		file.println(String(s->status.agc_gain));
		file.println(String(s->status.gainceiling));
		file.println(String(s->status.bpc));
		file.println(String(s->status.wpc));
		file.println(String(s->status.raw_gma));
		file.println(String(s->status.lenc));
		file.println(String(s->status.hmirror));
		file.println(String(s->status.dcw));
		file.println(String(s->status.colorbar));
	} else {
		file.println("Camera sensor not available");
	}
	file.println("");

	file.println(macAddress);

	file.close();
	Serial.println("Camera setup saved to SD card");
	return true;
}

RTC_DATA_ATTR bool dr_armed = false;
static esp_timer_handle_t dr_clear_timer = nullptr;
static constexpr uint64_t DOUBLE_RESET_WINDOW_US = 5000000ULL; // 5s

void clearDoubleResetFlag(void *arg) {
	dr_armed = false;
}

static void startDoubleResetTimer() { // chat helped here
	if (!dr_clear_timer) {
		esp_timer_create_args_t args = {
			.callback = &clearDoubleResetFlag,
			.arg = nullptr,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "dr_clear"};
		if (esp_timer_create(&args, &dr_clear_timer) != ESP_OK) {
			return;
		}
	}
	esp_timer_stop(dr_clear_timer);
	esp_timer_start_once(dr_clear_timer, DOUBLE_RESET_WINDOW_US);
}

bool detectDoubleReset() {
	if (dr_armed) {
		dr_armed = false;
		return true;
	}

	dr_armed = true;
	startDoubleResetTimer();
	return false;
}

bool isDoubleResetWindowActive() {
	return dr_armed;
}

bool saveCameraSettingsToPrefs(const String &json) {
	prefs.begin("camset", false);
	prefs.putString("json", json);
	prefs.end();
	return true;
}

bool loadCameraSettingsFromPrefs(String &json) {
	prefs.begin("camset", true);
	json = prefs.getString("json", "");
	prefs.end();
	return json.length() > 0;
}

bool saveCameraSettingsToSD(const String &json) {
	if (!sdReady) {
		sdInit();
		if (!sdReady) {
			return false;
		}
	}

	SD.remove("/cam_settings.json");
	File file = SD.open("/cam_settings.json", FILE_WRITE);
	if (!file) {
		Serial.println("Failed to open cam_settings.json for writing");
		return false;
	}

	file.print(json);
	file.close();
	return true;
}

bool loadCameraSettingsFromSD(String &json) {
	if (!sdReady) {
		sdInit();
		if (!sdReady) {
			return false;
		}
	}
	if (!SD.exists("/cam_settings.json")) {
		return false;
	}

	File file = SD.open("/cam_settings.json", FILE_READ);
	if (!file) {
		Serial.println("Failed to open cam_settings.json for reading");
		return false;
	}
	json = file.readString();
	file.close();
	return json.length() > 0;
}

static String nextRecordingFolder() {
	if (!SD.exists("/records")) {
		SD.mkdir("/records");
	}

	String folder = "/records/clip" + String(videoNumber);
	int attempts = 0;
	while (SD.exists(folder) && attempts < 1000) {
		videoNumber++;
		folder = "/records/clip" + String(videoNumber);
		attempts++;
	}

	if (!SD.mkdir(folder)) {
		return String("");
	}
	videoNumber++;
	saveVideoNumber(videoNumber);
	return folder;
}

bool recordMotionClip(uint32_t durationMs, String &savedFilePath) {
	if (!sdReady) {
		sdInit();
		if (!sdReady) {
			return false;
		}
	}

	String folder = nextRecordingFolder();
	if (folder.length() == 0) {
		Serial.println("Failed to create recording folder");
		return false;
	}

	String filePath = folder + "/video.mjpeg";
	File file;
	if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
		file = SD.open(filePath.c_str(), FILE_WRITE);
		xSemaphoreGive(sdMutex);
	}

	if (!file) {
		Serial.println("Failed to open MJPEG file for writing");
		return false;
	}

	uint32_t stopAt = millis() + durationMs;
	const uint32_t frameDelayMs = 80; // ~12 fps

	while (millis() < stopAt) {
		camera_fb_t *fb = nullptr;
		if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
			fb = captureFrame();
			xSemaphoreGive(cameraMutex);
		}

		if (fb) {
			if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
				file.print("--frame\r\nContent-Type: image/jpeg\r\n\r\n");
				file.write(fb->buf, fb->len);
				file.print("\r\n");
				xSemaphoreGive(sdMutex);
			}
			esp_camera_fb_return(fb);
		}

		vTaskDelay(pdMS_TO_TICKS(frameDelayMs));
	}

	if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
		file.close();
		xSemaphoreGive(sdMutex);
	}

	savedFilePath = filePath;
	Serial.printf("Motion clip saved: %s\n", savedFilePath.c_str());
	return true;
}
