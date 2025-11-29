// sd_storage.cpp
#include "sd_storage.h"
#include "SD.h"
#include "select_pins.h"
#include <Arduino.h>
#include <ESP.h>
#include <esp_camera.h>
#include <esp_timer.h>

bool sdReady = false;
Preferences prefs;

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
	// if(!sdReady) {
	// 	sdInit();
	// 	if(!sdReady) {
	// 		Serial.println("kamo ono to neotvara sd kartu");
	// 	}
	// } 
	// File file = SD.open("/photoNum.txt", FILE_WRITE);
	// if (!file) {
	// 	Serial.println("Failed to open file for writing");
	// 	return false;
	// }
	// file.println(num);
	// file.close();
	prefs.begin("camera", false);
	prefs.putInt("num", num);
	prefs.end();
	return true;
}

uint16_t loadPhotoNumber() {
	// if(!sdReady) {
	// 	sdInit();
	// 	if(!sdReady) {
	// 		Serial.println("kamo ono to neotvara sd kartu");
	// 	}
	// } 
	// if (!SD.exists("/photoNum.txt")) {
	// 	return 0;
	// }

	// File file = SD.open("/photoNum.txt", FILE_READ);
	// if (!file) {
	// 	Serial.println("Failed to open file for reading");
	// 	return 999;
	// }

	// int num = file.parseInt();
	// file.close();
	prefs.begin("camera", true);
	int num = prefs.getInt("num", 10000);
	prefs.end();
	return num;
}

bool loadWIFICredentials(String &ssid, String &password) {
	// if(!sdReady) {
	// 	sdInit();
	// 	if(!sdReady) {
	// 		Serial.println("kamo ono to neotvara sd kartu");
	// 	}
	// } 
	// if (!SD.exists("/wifiCr.txt")) {
	// 	Serial.println("wifi.txt does not exist on SD");
	// 	return false;
	// }

	// File file = SD.open("/wifiCr.txt", FILE_READ);
	// if (!file) {
	// 	Serial.println("Failed to open wifi.txt for reading");
	// 	return false;
	// }

	// ssid = file.readStringUntil('\n');
	// ssid.trim();
	// password = file.readStringUntil('\n');
	// password.trim();

	// file.close();
	prefs.begin("wifi", true);
	ssid = prefs.getString("ssid", "");
	password = prefs.getString("password", "");
	bool hasCredentials = ssid.length() > 0 && password.length() > 0;
	prefs.end();
	return hasCredentials;
}

bool firstTime() {
	// if(!sdReady) {
	// 	sdInit();
	// 	if(!sdReady) {
	// 		Serial.println("kamo ono to neotvara sd kartu");
	// 	}
	// } 
	//return !SD.exists("/camSet.txt");
	prefs.begin("wifi", true);
	bool firstBoot = prefs.getBool("first", true);
	prefs.end();
	return firstBoot;
}

bool saveWIFICredentials(String &ssid, String &password) {
	// if(!sdReady) {
	// 	sdInit();
	// 	if(!sdReady) {
	// 		Serial.println("kamo ono to neotvara sd kartu");
	// 	}
	// } 
	// File file = SD.open("/wifiCr.txt", FILE_WRITE);
	// if (!file) {
	// 	Serial.println("Failed to open wifiCr.txt for writing");
	// 	return false;
	// }
	// file.println(ssid);
	// file.println(password);
	// file.close();
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

// RTC-backed double-reset detector (non-blocking, 2.5s window)
RTC_DATA_ATTR uint64_t dr_last_boot_us = 0;
RTC_DATA_ATTR bool dr_armed = false;
bool detectDoubleReset() {
	uint64_t now = esp_timer_get_time(); // microseconds since boot
	bool withinWindow = dr_armed && (now - dr_last_boot_us) < 2500000ULL; // 2.5s
	bool detected = withinWindow;
	// re-arm for next boot
	dr_armed = true;
	dr_last_boot_us = now;
	return detected;
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
