#include "connected_devices.h"

Adafruit_MCP23X17 mcp;
RTC_DS3231 rtc;
bool rtcReady = false;
Adafruit_BME680 bme;
bool bme680Ready = false;
int currentXMotorAngle = 0; // <->
int currentYMotorAngle = 0; // ^ v

void printTime(DateTime &now) {
	Serial.print("\r");
	Serial.print(now.year(), DEC);
	Serial.print('/');
	Serial.print(now.month(), DEC);
	Serial.print('/');
	Serial.print(now.day(), DEC);
	Serial.print(" ");
	Serial.print(now.hour(), DEC);
	Serial.print(':');
	Serial.print(now.minute(), DEC);
	Serial.print(':');
	Serial.println(now.second(), DEC);
}

String stringTime(DateTime &now) {
	String cas = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + "   " + String(now.day()) + ". " + String(now.month()) + ". " + String(now.year());
	return cas;
}

void setRTCTime() {
	if (!rtcReady) {
		return;
	}

	rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void setMotorAngle(int angleX, int angleY) {
	int steps = (abs(currentXMotorAngle - angleX) * 512) / 360;
	// 512 == 360 degrees

	if (currentXMotorAngle > angleX) {
		for (int j = 0; j < steps; j++) {
			mcp.digitalWrite(OUTPUT1X, HIGH);
			mcp.digitalWrite(OUTPUT2X, LOW);
			mcp.digitalWrite(OUTPUT3X, LOW);
			mcp.digitalWrite(OUTPUT4X, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1X, LOW);
			mcp.digitalWrite(OUTPUT2X, LOW);
			mcp.digitalWrite(OUTPUT3X, HIGH);
			mcp.digitalWrite(OUTPUT4X, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1X, LOW);
			mcp.digitalWrite(OUTPUT2X, HIGH);
			mcp.digitalWrite(OUTPUT3X, HIGH);
			mcp.digitalWrite(OUTPUT4X, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1X, HIGH);
			mcp.digitalWrite(OUTPUT2X, HIGH);
			mcp.digitalWrite(OUTPUT3X, LOW);
			mcp.digitalWrite(OUTPUT4X, LOW);
			delay(DELAY);
		}
	} else {
		for (int i = 0; i < steps; i++) {
			mcp.digitalWrite(OUTPUT1X, HIGH);
			mcp.digitalWrite(OUTPUT2X, HIGH);
			mcp.digitalWrite(OUTPUT3X, LOW);
			mcp.digitalWrite(OUTPUT4X, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1X, LOW);
			mcp.digitalWrite(OUTPUT2X, HIGH);
			mcp.digitalWrite(OUTPUT3X, HIGH);
			mcp.digitalWrite(OUTPUT4X, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1X, LOW);
			mcp.digitalWrite(OUTPUT2X, LOW);
			mcp.digitalWrite(OUTPUT3X, HIGH);
			mcp.digitalWrite(OUTPUT4X, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1X, HIGH);
			mcp.digitalWrite(OUTPUT2X, LOW);
			mcp.digitalWrite(OUTPUT3X, LOW);
			mcp.digitalWrite(OUTPUT4X, HIGH);
			delay(DELAY);
		}
	}
	currentXMotorAngle = angleX;

	// y min -90 -> max 90
	steps = (abs(currentYMotorAngle - angleY) * 512) / 360;
	// 512 == 360 degrees
	if (currentYMotorAngle > angleY) {
		for (int j = 0; j < steps; j++) {
			mcp.digitalWrite(OUTPUT1Y, HIGH);
			mcp.digitalWrite(OUTPUT2Y, LOW);
			mcp.digitalWrite(OUTPUT3Y, LOW);
			mcp.digitalWrite(OUTPUT4Y, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1Y, LOW);
			mcp.digitalWrite(OUTPUT2Y, LOW);
			mcp.digitalWrite(OUTPUT3Y, HIGH);
			mcp.digitalWrite(OUTPUT4Y, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1Y, LOW);
			mcp.digitalWrite(OUTPUT2Y, HIGH);
			mcp.digitalWrite(OUTPUT3Y, HIGH);
			mcp.digitalWrite(OUTPUT4Y, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1Y, HIGH);
			mcp.digitalWrite(OUTPUT2Y, HIGH);
			mcp.digitalWrite(OUTPUT3Y, LOW);
			mcp.digitalWrite(OUTPUT4Y, LOW);
			delay(DELAY);
		}
	} else {
		for (int i = 0; i < steps; i++) {
			mcp.digitalWrite(OUTPUT1Y, HIGH);
			mcp.digitalWrite(OUTPUT2Y, HIGH);
			mcp.digitalWrite(OUTPUT3Y, LOW);
			mcp.digitalWrite(OUTPUT4Y, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1Y, LOW);
			mcp.digitalWrite(OUTPUT2Y, HIGH);
			mcp.digitalWrite(OUTPUT3Y, HIGH);
			mcp.digitalWrite(OUTPUT4Y, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1Y, LOW);
			mcp.digitalWrite(OUTPUT2Y, LOW);
			mcp.digitalWrite(OUTPUT3Y, HIGH);
			mcp.digitalWrite(OUTPUT4Y, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1Y, HIGH);
			mcp.digitalWrite(OUTPUT2Y, LOW);
			mcp.digitalWrite(OUTPUT3Y, LOW);
			mcp.digitalWrite(OUTPUT4Y, HIGH);
			delay(DELAY);
		}
	}
	currentYMotorAngle = angleY;
}

void setupSensors() {
	bme680Ready = false;
	if (bme.begin(0x77, &Wire)) {
		bme680Ready = true;
	}

	if (bme680Ready) {
		bme.setTemperatureOversampling(BME680_OS_8X);
		bme.setHumidityOversampling(BME680_OS_2X);
		bme.setPressureOversampling(BME680_OS_4X);
		bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
		bme.setGasHeater(320, 150); // 320°C for 150 ms

		if (!bme.performReading()) {
			Serial.println("WARNING: BME680 detected but failed to perform reading");
			bme680Ready = false;
		}
	} else {
		Serial.println("WARNING: BME680 not found at address 0x77");
	}

	if (!rtc.begin(&Wire)) {
		rtcReady = false;
	} else {
		rtcReady = true;
	}

	mcp.pinMode(MCP_PIR_PIN, INPUT);

	// motor X
	mcp.pinMode(OUTPUT1X, OUTPUT);
	mcp.pinMode(OUTPUT2X, OUTPUT);
	mcp.pinMode(OUTPUT3X, OUTPUT);
	mcp.pinMode(OUTPUT4X, OUTPUT);

	// motor Y
	mcp.pinMode(OUTPUT1Y, OUTPUT);
	mcp.pinMode(OUTPUT2Y, OUTPUT);
	mcp.pinMode(OUTPUT3Y, OUTPUT);
	mcp.pinMode(OUTPUT4Y, OUTPUT);
}