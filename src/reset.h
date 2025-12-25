#pragma once

#include <Arduino.h>

// Reset helpers for power-cycle detection and clearing persisted config.
constexpr uint32_t DOUBLE_RESET_POWER_WINDOW_SECONDS = 5;

bool detectPowerCycleDoubleReset();
void factoryReset();
bool ensureMotorReadyForIndicator();
void indicateDoubleResetWindow();
