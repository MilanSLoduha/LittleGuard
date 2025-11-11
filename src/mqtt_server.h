#pragma once
#include <Arduino.h>
#include "modem.h"
#include "HivemqRootCA.h"

bool mqtt_connect_manualLTE();
void mqtt_callback(const char *topic, const uint8_t *payload, uint32_t len);
void mqttPrepareLTE();