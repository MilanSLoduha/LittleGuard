#pragma once

// Musí byť pred TinyGsmClient.h!
#define TINY_GSM_MODEM_A7670
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>
#include <StreamDebugger.h>
#include "secrets.h"
#include <Arduino.h>
#include "select_pins.h"

extern StreamDebugger debugger;
extern TinyGsm modem;

extern const char *broker_host;
extern const uint16_t broker_port;
extern const char *broker_username;
extern const char *broker_password;
extern const char *client_id;
extern const char *temperature_topic;
extern const char *motion_topic;
extern const uint8_t mqtt_client_id;
extern uint32_t check_connect_millis;  // Pre MQTT reconnect timer

void setupModem();
void initSIM();
void connectMobileData();
