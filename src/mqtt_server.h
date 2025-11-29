#pragma once
#include <Arduino.h>
#include "HivemqRootCA.h"
#include "secrets.h"
#include "camera.h"
#include "sd_storage.h"

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
