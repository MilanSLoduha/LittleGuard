#pragma once
#include "HivemqRootCA.h"
#include "camera.h"
#include "sd_storage.h"
#include "secrets.h"
#include <Arduino.h>
#include <base64.h>

struct CameraSettings {
	String mode;
	String resolution;
	int quality;
	int brightness;
	int contrast;
	int motorX;
	int motorY;
	bool hFlip;
	bool vFlip;
	bool hwDownscale;
	bool awb;
	bool aec;
	String phoneNumber;
	String emailAddress;
	bool sendSMS;
	bool sendEmail;
	bool monday;
	bool tuesday;
	bool wednesday;
	bool thursday;
	bool friday;
	bool saturday;
	bool sunday;
	String startTime;
	String endTime;
	int sensorInterval;
};

extern CameraSettings currentSettings;

extern const char *broker_host;
extern const uint16_t broker_port;
extern const char *broker_username;
extern const char *broker_password;
extern const char *client_id;

extern const uint8_t mqtt_client_id;

extern bool stream;

bool mqtt_connect_manualLTE();
void mqtt_callback(const char *topic, const uint8_t *payload, uint32_t len);
void mqtt_callback_wrapper(char *topic, uint8_t *payload, unsigned int len);
void mqttPrepareLTE();
void initCameraSettings();
bool ensureWifiMqtt();
bool publishMQTT(String topic, String message);
bool postFrame();
bool connectAbly();
void setMotorAngle(int angleX, int angleY);
void publishSettingsState();
bool sendSMSNotification(String phoneNumber, String message);
