#include "reset.h"

#include <ESP.h>
#include <Preferences.h>
#include <RTClib.h>
#include <Wire.h>

#include "connected_devices.h"
#include "select_pins.h"
#include "sd_storage.h"

RTC_DATA_ATTR uint32_t doubleResetCounter = 0;
extern bool mcpReady;

bool ensureMotorReadyForIndicator() {
	if (!mcpReady) {
		Wire.begin(SDA_PIN, SCL_PIN);
		if (!mcp.begin_I2C(0x20)) {
			Serial.println("MCP23017 not found for reset indicator");
			return false;
		}
		setupMotorPins();
		mcpReady = true;
	}
	return true;
}

void indicateDoubleResetWindow() {
	if (!isDoubleResetWindowActive()) {
		return;
	}
	if (!ensureMotorReadyForIndicator()) {
		return;
	}

	int baseX = currentXMotorAngle;
	int startY = currentYMotorAngle;
	int up = constrain(startY + 10, -90, 90);

	const unsigned long safetyMs = DOUBLE_RESET_POWER_WINDOW_SECONDS * 1000UL + 1500; // cap in case timer fails
	unsigned long startMs = millis();
	while (isDoubleResetWindowActive() && (millis() - startMs) < safetyMs) {
		setMotorAngle(baseX, up);
		delay(150);
		setMotorAngle(baseX, startY);
		delay(150);
	}
}

bool detectPowerCycleDoubleReset() {
	Preferences drPrefs;
	drPrefs.begin("dr", false);

	uint32_t storedTs = drPrefs.getULong("ts", 0);
	uint32_t nowTs = 0;
	bool rtcOk = false;

	Wire.begin(SDA_PIN, SCL_PIN);
	if (rtc.begin(&Wire)) {
		DateTime now = rtc.now();
		nowTs = now.unixtime();
		rtcOk = true;
	}

	bool detected = false;
	if (rtcOk && storedTs > 0 && nowTs > storedTs && (nowTs - storedTs) <= DOUBLE_RESET_POWER_WINDOW_SECONDS) {
		detected = true;
	}

	if (rtcOk) {
		if (detected) {
			drPrefs.putULong("ts", 0);
		} else {
			drPrefs.putULong("ts", nowTs);
		}
	}

	drPrefs.end();
	return detected;
}

void factoryReset() {
	Preferences resetPrefs;

	resetPrefs.begin("wifi", false);
	resetPrefs.clear();
	resetPrefs.end();

	resetPrefs.begin("camera", false);
	resetPrefs.clear();
	resetPrefs.end();

	resetPrefs.begin("setup", false);
	resetPrefs.clear();
	resetPrefs.end();

	doubleResetCounter = 0;
	delay(150);
	ESP.restart();
}
