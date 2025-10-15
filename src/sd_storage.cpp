// sd_storage.cpp
#include <Arduino.h>
#include "sd_storage.h"
#include "select_pins.h"
#include "SD.h"

bool sdInit() {
  Serial.println("--> Trying SD (SPI) init ...");
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, SPI)) {
    Serial.println("SD.begin() failed");
    return false;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }
  Serial.println("SD initialized OK (SPI)");
  return true;
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
