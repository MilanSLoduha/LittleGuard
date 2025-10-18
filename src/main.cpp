// main.cpp - top-level orchestration
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <RTClib.h>
#include "select_pins.h"
#include "sd_storage.h"
#include "camera.h"
#include "bit_banging.h"
#include "rtc_time.h"
Adafruit_MCP23X17 mcp;
RTC_DS3231 rtc;

int lastMotionStatus = -1;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("T-SIMCAM: Inicializácia I2C na pinoch GPIO43 (SDA) a GPIO44 (SCL)");

  // Inicializácia I2C pre MCP23017
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inicializácia MCP23017
  if (!mcp.begin_I2C(0x20)) {
    Serial.println("MCP23017 nebol nájdený na adrese 0x20! Skontroluj zapojenie alebo adresu.");
    while (1) delay(1000);
  }
  mcp.pinMode(MCP_SDA_PIN, OUTPUT); // GPA0 ako SDA pre DS3231
  mcp.pinMode(MCP_SCL_PIN, OUTPUT); // GPA1 ako SCL pre DS3231
  mcp.digitalWrite(MCP_SDA_PIN, HIGH); // Simulácia pull-up
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  Serial.println("MCP23017 pripravený pre bit-banging I2C k DS3231.");

  mcp.pinMode(MCP_PIR_PIN, INPUT);

}

void loop() {
  DateTime now;
  //printTime(now);
  
  int pirState = mcp.digitalRead(MCP_PIR_PIN);
  
  if(lastMotionStatus != pirState) {
    
    if(pirState == HIGH) {
      Serial.println("Pohyb detekovaný!");
    } 
    else {
      Serial.println("Žiadny pohyb.");
    }
    
    lastMotionStatus = pirState;
  }

  delay(400);
  Serial.clearWriteError();
}



/* ///////////////////////////////////////// I2C Scanner cez MCP23017 ////////////////////////////////////////

#include <Wire.h>
#include <Adafruit_MCP23X17.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(43, 44); // SDA=GPIO43, SCL=GPIO44
  Serial.println("I2C Scanner");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Skenujem...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C zariadenie na adrese 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) Serial.println("Žiadne I2C zariadenia!");
  delay(5000);
}
*/ ///////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ////////////////////////////////////////////// Test Camera ////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("T-SIMCAM minimal startup");

  if (!setupCamera()) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }

  if (!sdInit()) {
    Serial.println("SD init failed - continuing without SD");
  }

  camera_fb_t *fb = captureFrame();
  if (!fb) {
    Serial.println("Camera capture failed (fb null)");
    return;
  }

  sdWritePhoto("/photo.jpg", fb);
  esp_camera_fb_return(fb);
  Serial.println("Capture done.");
}

void loop() {
  Serial.print(".");
  delay(5000);
}*//////////////////////////////////////////////////////////////////////////////////////////////////

/* /////////////////////////////////////////////// Test PIR //////////////////////////////////////////////////
int lastMotionStatus = -1;


void setup() {
  Serial.begin(115200);
  delay(10000);

  Serial.println("T-SIMCAM: Test PIR senzora na MCP23017 (GPA2)");

  Wire.begin(43, 44); // SDA=43, SCL=44 (Grove konektor na T-SIMCAM)

  if (!mcp.begin_I2C(MCP_ADDRESS)) {
    Serial.println("MCP23017 nebol nájdený! Skontroluj zapojenie a adresu.");
    while (true) delay(100);
  }

  mcp.pinMode(MCP_PIR_PIN, INPUT);
  Serial.println("MCP23017 inicializovaný. PIR senzor pripravený.");
}

void loop() {
  int pirState = mcp.digitalRead(MCP_PIR_PIN);

  if(lastMotionStatus != pirState) {

    if(pirState == HIGH) {
      Serial.print("Pohyb detekovaný!");
    } 
    else {
      Serial.print("Žiadny pohyb.");
    }

    lastMotionStatus = pirState;
  }

  delay(1000);
}
*//////////////////////////////////////////////////////////////////////////////////////////////////