#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

struct InaValues;

class WebInterface
{
public:
  WebInterface();

  bool begin(const char *ssid, const char *password);
  IPAddress localIp() const;
  bool isConnected() const;
  void updateMeasurements(const InaValues &values);
  void loop();

private:
  String buildPage() const;

  ESP8266WebServer m_server;
  String           m_lastMeasurementHtml;
  bool             m_webReady;
  bool             m_connected;
  IPAddress        m_localIp;
  float            m_lastEnergyWs;
};
