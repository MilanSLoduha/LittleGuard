#pragma once

// Musí byť pred TinyGsmClient.h!
#define TINY_GSM_MODEM_A7670
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>
#include <StreamDebugger.h>
#include "secrets.h"
#include <Arduino.h>
#include "select_pins.h"
#include "mqtt_server.h"

extern StreamDebugger debugger;
extern TinyGsm modem;

extern const uint8_t mqtt_client_id;
extern uint32_t check_connect_millis;  // MQTT reconnect timer

void setupModem();
void initSIM();
bool connectMobileData();
