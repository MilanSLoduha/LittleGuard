#include "rtc_time.h"
#include <RTClib.h>

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
	String cas = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + " " + String(now.day()) + ". " + String(now.month()) + ". " + String(now.year());
	return cas;
}