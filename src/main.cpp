// main.cpp - top-level orchestration
#include <Arduino.h>
/*#include "camera.h"
#include "sd_storage.h"

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
}*/
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <RTClib.h>

#define SDA_PIN 43  // Grove RX (cerveny kábel)
#define SCL_PIN 44  // Grove TX (biely kábel)
#define MCP_SDA_PIN 0  // GPA0 na MCP23017 (SDA pre DS3231)
#define MCP_SCL_PIN 1  // GPA1 na MCP23017 (SCL pre DS3231)
#define MCP_PIR_PIN 2  // GPA2 na MCP23017 (PIR senzor)

Adafruit_MCP23X17 mcp;
RTC_DS3231 rtc; // Použité pre DateTime objekt, aj keď je I2C simulované

// Funkcie pre bit-banging I2C cez MCP23017
void i2c_start() {
  mcp.digitalWrite(MCP_SDA_PIN, HIGH);
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SDA_PIN, LOW);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SCL_PIN, LOW);
}

void i2c_stop() {
  mcp.digitalWrite(MCP_SDA_PIN, LOW);
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SDA_PIN, HIGH);
  delayMicroseconds(5);
}

void i2c_write_bit(uint8_t bit) {
  mcp.digitalWrite(MCP_SDA_PIN, bit);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  mcp.digitalWrite(MCP_SCL_PIN, LOW);
}

uint8_t i2c_read_bit() {
  uint8_t bit;
  mcp.digitalWrite(MCP_SCL_PIN, HIGH);
  delayMicroseconds(5);
  bit = mcp.digitalRead(MCP_SDA_PIN);
  mcp.digitalWrite(MCP_SCL_PIN, LOW);
  return bit;
}

bool i2c_write_byte(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    i2c_write_bit((byte >> i) & 1);
  }
  mcp.pinMode(MCP_SDA_PIN, INPUT); // Na čítanie ACK
  uint8_t ack = i2c_read_bit();
  mcp.pinMode(MCP_SDA_PIN, OUTPUT); // Späť na výstup
  return ack == 0;
}

uint8_t i2c_read_byte(bool nack) {
  uint8_t byte = 0;
  mcp.pinMode(MCP_SDA_PIN, INPUT);
  for (int i = 7; i >= 0; i--) {
    byte |= i2c_read_bit() << i;
  }
  mcp.pinMode(MCP_SDA_PIN, OUTPUT);
  i2c_write_bit(nack);
  return byte;
}

bool read_rtc_time(DateTime &dt) {
  i2c_start();
  if (!i2c_write_byte(0x68 << 1)) { // DS3231 adresa + write bit
    i2c_stop();
    Serial.println("Chyba: DS3231 neodpovedá!");
    return false;
  }
  i2c_write_byte(0x00); // Začať od registra 0 (sekundy)
  i2c_stop();

  i2c_start();
  if (!i2c_write_byte((0x68 << 1) | 1)) { // DS3231 adresa + read bit
    i2c_stop();
    Serial.println("Chyba: DS3231 neodpovedá pri čítaní!");
    return false;
  }
  uint8_t seconds = i2c_read_byte(false);
  uint8_t minutes = i2c_read_byte(false);
  uint8_t hours = i2c_read_byte(false);
  uint8_t day = i2c_read_byte(false);
  uint8_t date = i2c_read_byte(false);
  uint8_t month = i2c_read_byte(false);
  uint8_t year = i2c_read_byte(true);
  i2c_stop();

  // Dekódovanie BCD do decimálneho
  dt = DateTime(
    2000 + (year >> 4) * 10 + (year & 0x0F),
    (month >> 4) * 10 + (month & 0x0F),
    (date >> 4) * 10 + (date & 0x0F),
    (hours >> 4) * 10 + (hours & 0x0F),
    (minutes >> 4) * 10 + (minutes & 0x0F),
    (seconds >> 4) * 10 + (seconds & 0x0F)
  );
  return true;
}



int motionCount = 0;
int noMotionCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Test PIR HC-SR501 na Grove (pin 46)");

  pinMode(SCL_PIN, INPUT);  // Nastav pin ako vstup
  Serial.println("PIR senzor pripravený. Pohyb rukou pred senzorom na test.");
}

void loop() {
  int pirState = digitalRead(SCL_PIN);
  
  if (pirState == HIGH) {
    Serial.print("Pohyb detekovaný! (HIGH) - Počet: ");
    Serial.println(motionCount++);
  } else {
    Serial.print("Žiadny pohyb (LOW) - Počet: ");
    Serial.println(noMotionCount++);
  }
  
  delay(500);  // Čítaj každých 500 ms
}
/*
int ano = 0;
int nie = 0;

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
  /*if (read_rtc_time(now)) {
    Serial.print("Aktuálny čas: ");
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.println(now.second(), DEC);
  } else {
    Serial.println("Chyba pri čítaní času z DS3231!");
  }
  delay(400);
  if(mcp.digitalRead(MCP_PIR_PIN) == HIGH) {
    Serial.print("ano: ");
    Serial.println(ano);
    ano++;
  } else {
    Serial.print("nie: ");
    Serial.println(nie);
    nie++;
  }
}
*/


/*

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
  */
 /*
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\nSerial test - ESP32 running!");
}

void loop() {
  Serial.println("Still alive...");
  delay(1000);
}
*/
