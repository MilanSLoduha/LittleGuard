#pragma once
#include "select_pins.h"
#include <Adafruit_MCP23X17.h>
#include <RTClib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#define sensor_t adafruit_sensor_t
#include "Adafruit_BME680.h"
#undef sensor_t

// Mutexes for shared resources
extern SemaphoreHandle_t i2cMutex;
extern SemaphoreHandle_t sdMutex;
extern SemaphoreHandle_t cameraMutex;
extern SemaphoreHandle_t modemMutex;

// Queues for inter-task communication
extern QueueHandle_t snapshotRequestQueue;
extern QueueHandle_t notificationQueue;
extern QueueHandle_t motorCommandQueue;

// Task handles
extern TaskHandle_t networkTaskHandle;
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t streamTaskHandle;

void initSharedResources();

void printTime(DateTime &now);
String stringTime(DateTime &now);
void setRTCTime();
void setMotorAngle(int angleX, int angleY);
void setupMotorPins();
void setupSensors();
bool isNotificationAllowed();

extern Adafruit_MCP23X17 mcp;

extern RTC_DS3231 rtc;
extern bool rtcReady;

extern Adafruit_BME680 bme;
extern bool bme680Ready;

extern int currentXMotorAngle; // <->
extern int currentYMotorAngle; // ^ v
