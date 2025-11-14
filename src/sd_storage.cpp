// sd_storage.cpp
#include "sd_storage.h"
#include "SD.h"
#include "select_pins.h"
#include <Arduino.h>

bool sdReady = false;

void sdInit() {
	Serial.println("--> Trying SD (SPI) init ...");
	SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
	if (!SD.begin(SD_CS_PIN, SPI)) {
		sdReady = false;
		Serial.println("SD.begin() failed");
	}
	uint8_t cardType = SD.cardType();
	if (cardType == CARD_NONE) {
		sdReady = false;
		Serial.println("No SD card attached");
	}
	Serial.println("SD initialized OK (SPI)");
	sdReady = true;
	photoNumber = loadPhotoNumber();
}

bool sdWritePhoto(const char *path, camera_fb_t *fb) {
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
	File file = SD.open("/photoNum.txt", FILE_WRITE);
	if (!file) {
		Serial.println("Failed to open file for writing");
		return false;
	}
	file.println(num);
	file.close();
	return true;
}

uint16_t loadPhotoNumber() {
	if (!SD.exists("/photoNum.txt")) {
		return 0;
	}

	File file = SD.open("/photoNum.txt", FILE_READ);
	if (!file) {
		Serial.println("Failed to open file for reading");
		return 999;
	}

	int num = file.parseInt();
	file.close();
	return num;
}
