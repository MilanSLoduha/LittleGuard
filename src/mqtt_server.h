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
extern const char *temperature_topic;
extern const char *motion_topic;
extern const char *last_motion_topic;
extern const char *command_topic;
extern const char *settings_topic;
extern const char *stream_topic;
extern const char *snapshot_topic;

extern const uint8_t mqtt_client_id;

bool mqtt_connect_manualLTE();
void mqtt_callback(const char *topic, const uint8_t *payload, uint32_t len);
void mqttPrepareLTE();