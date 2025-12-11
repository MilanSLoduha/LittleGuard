#pragma once
#include <Adafruit_MCP23X17.h>
#include <RTClib.h>
#include "select_pins.h"
#define sensor_t adafruit_sensor_t
#include "Adafruit_BME680.h"
#undef sensor_t


void printTime(DateTime &now);
String stringTime(DateTime &now);
void setRTCTime();
void setMotorAngle(int angleX, int angleY);
void setupMotorPins();
void setupSensors();

extern Adafruit_MCP23X17 mcp;

extern RTC_DS3231 rtc;
extern bool rtcReady;

extern Adafruit_BME680 bme;
extern bool bme680Ready;

extern int currentXMotorAngle; // <->
extern int currentYMotorAngle; // ^ v 
