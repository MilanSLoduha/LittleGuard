#pragma once
#include <Adafruit_MCP23X17.h>
#include <RTClib.h>

bool read_rtc_time(DateTime &dt);
void printTime(DateTime &now);
String stringTime(DateTime &now);