#pragma once
#include <Adafruit_MCP23X17.h>
#include <RTClib.h>

void printTime(DateTime &now);
String stringTime(DateTime &now);