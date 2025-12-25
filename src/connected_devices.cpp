#include "connected_devices.h"
#include "mqtt_server.h"

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

bool isNotificationAllowed() {
	if (!rtcReady) {
		return true; // Ak RTC nefunguje, povoliť upozornenia
	}

	DateTime now = rtc.now();
	int dayOfWeek = now.dayOfTheWeek(); // 0=Sunday, 1=Monday, ..., 6=Saturday

	// Kontrola dňa
	bool dayAllowed = false;
	switch (dayOfWeek) {
	case 0:
		dayAllowed = currentSettings.sunday;
		break;
	case 1:
		dayAllowed = currentSettings.monday;
		break;
	case 2:
		dayAllowed = currentSettings.tuesday;
		break;
	case 3:
		dayAllowed = currentSettings.wednesday;
		break;
	case 4:
		dayAllowed = currentSettings.thursday;
		break;
	case 5:
		dayAllowed = currentSettings.friday;
		break;
	case 6:
		dayAllowed = currentSettings.saturday;
		break;
	}

	// Ak žiadny deň nie je nastavený, povoliť všetky dni
	bool anyDaySet =
	    currentSettings.monday || currentSettings.tuesday || currentSettings.wednesday || currentSettings.thursday || currentSettings.friday || currentSettings.saturday || currentSettings.sunday;
	if (!anyDaySet) {
		dayAllowed = true;
	}

	if (!dayAllowed) {
		return false;
	}

	// Kontrola času
	int currentMinutes = now.hour() * 60 + now.minute();

	// Parsovanie startTime a endTime (formát "HH:MM")
	int startHour = 0, startMin = 0, endHour = 23, endMin = 59;

	if (currentSettings.startTime.length() >= 5) {
		startHour = currentSettings.startTime.substring(0, 2).toInt();
		startMin = currentSettings.startTime.substring(3, 5).toInt();
	}

	if (currentSettings.endTime.length() >= 5) {
		endHour = currentSettings.endTime.substring(0, 2).toInt();
		endMin = currentSettings.endTime.substring(3, 5).toInt();
	}

	int startMinutes = startHour * 60 + startMin;
	int endMinutes = endHour * 60 + endMin;

	// Kontrola časového rozmedzí (aj cez polnoc)
	if (startMinutes <= endMinutes) {
		// Normálny prípad (napr. 09:00 - 17:00)
		return (currentMinutes >= startMinutes && currentMinutes <= endMinutes);
	} else {
		// Cez polnoc (napr. 22:00 - 02:00)
		return (currentMinutes >= startMinutes || currentMinutes <= endMinutes);
	}
}

void setupMotorPins() {
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

	setupMotorPins();
}
