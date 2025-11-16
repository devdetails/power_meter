#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <IPAddress.h>

struct InaValues;

class DisplayManager
{
public:
  DisplayManager();

  bool begin();
  void showConnecting(const char *ssid);
  void showMeasurements(const InaValues &values, bool sensorOk, bool webConnected, const IPAddress &ip);

private:
  Adafruit_SH1107 m_display;
  bool            m_ready;
};
