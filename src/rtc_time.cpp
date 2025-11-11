#include <RTClib.h>
#include "rtc_time.h"

void printTime(DateTime &now) {
  if(read_rtc_time(now)) {
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
  else {
    Serial.println("Chyba pri čítaní času z DS3231!");
  }
}

String stringTime(DateTime &now) {
  String cas = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + " " + String(now.day()) + ". " + String(now.month()) + ". " + String(now.year());
  return cas;
}