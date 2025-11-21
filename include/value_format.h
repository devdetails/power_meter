#pragma once

#include <Arduino.h>

struct ValuePrefix
{
  const char *symbol;
  float       factor;
};

ValuePrefix findValuePrefix(float absValue);
String      formatValue(float baseValue, const char *baseUnit, uint8_t digits);
String      formatTime(float seconds);
