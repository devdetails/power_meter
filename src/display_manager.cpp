#include "display_manager.h"

#include "ina_values.h"

namespace
{
constexpr uint8_t  SH1107_ADDR   = 0x3C;
constexpr uint16_t SH1107_WIDTH  = 128;
constexpr uint16_t SH1107_HEIGHT = 128;
}

DisplayManager::DisplayManager()
    : m_display(SH1107_HEIGHT, SH1107_WIDTH, &Wire, -1)
    , m_ready(false)
{
}

bool DisplayManager::begin()
{
  if (!m_display.begin(SH1107_ADDR, false))
  {
    Serial.println("SH1107 OLED could not be initialized.");
    m_ready = false;
    return false;
  }

  m_display.clearDisplay();
  m_display.setRotation(3); // 90 deg counter-clockwise
  m_display.setTextSize(1);
  m_display.setTextColor(SH110X_WHITE);
  m_display.setCursor(0, 0);
  m_display.println(F("SH1107 init OK"));
  m_display.display();
  m_ready = true;
  return true;
}

void DisplayManager::showConnecting(const char *ssid)
{
  if (!m_ready)
  {
    return;
  }

  m_display.clearDisplay();
  m_display.setCursor(0, 0);
  m_display.print(F("connecting to "));
  m_display.println(ssid);
  m_display.display();
}

void DisplayManager::showMeasurements(const InaValues &values, bool sensorOk, bool webConnected, const IPAddress &ip)
{
  if (!m_ready)
  {
    return;
  }

  m_display.clearDisplay();
  m_display.setCursor(0, 0);

  if (!sensorOk)
  {
    m_display.println(F("INA228 error"));
  }
  else
  {
    m_display.println(F("INA228 values"));
    m_display.print(F("Vbus : "));
    m_display.print(values.vBus, 2);
    m_display.println(F(" V"));
    m_display.print(F("Vshunt: "));
    m_display.print(values.vShunt, 4);
    m_display.println(F(" V"));
    m_display.print(F("Temp : "));
    m_display.print(values.temperature, 1);
    m_display.println(F(" C"));
    m_display.print(F("Ishunt: "));
    m_display.print(values.current_mA, 2);
    m_display.println(F(" mA"));
  }

  m_display.setCursor(0, SH1107_HEIGHT - 8);
  m_display.print(F("IP: "));
  if (webConnected)
  {
    m_display.print(ip.toString());
  }
  else
  {
    m_display.print(F("Not connected"));
  }

  m_display.display();
}
