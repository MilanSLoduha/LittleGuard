#include <RTClib.h>
#include "bit_banging.h"
#include "rtc_time.h"

bool read_rtc_time(DateTime &dt) {
  i2c_start();
  if (!i2c_write_byte(0x68 << 1)) { // DS3231 adresa + write bit
    i2c_stop();
    Serial.println("Chyba: DS3231 neodpovedá!");
    return false;
  }
  i2c_write_byte(0x00); // Začať od registra 0 (sekundy)
  i2c_stop();

  i2c_start();
  if (!i2c_write_byte((0x68 << 1) | 1)) { // DS3231 adresa + read bit
    i2c_stop();
    Serial.println("Chyba: DS3231 neodpovedá pri čítaní!");
    return false;
  }
  uint8_t seconds = i2c_read_byte(false);
  uint8_t minutes = i2c_read_byte(false);
  uint8_t hours = i2c_read_byte(false);
  uint8_t day = i2c_read_byte(false);
  uint8_t date = i2c_read_byte(false);
  uint8_t month = i2c_read_byte(false);
  uint8_t year = i2c_read_byte(true);
  i2c_stop();

  // Dekódovanie BCD do decimálneho
  dt = DateTime(
    2000 + (year >> 4) * 10 + (year & 0x0F),
    (month >> 4) * 10 + (month & 0x0F),
    (date >> 4) * 10 + (date & 0x0F),
    (hours >> 4) * 10 + (hours & 0x0F),
    (minutes >> 4) * 10 + (minutes & 0x0F),
    (seconds >> 4) * 10 + (seconds & 0x0F)
  );
  return true;
}

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