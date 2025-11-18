#include "display_manager.h"

#include "ina_values.h"
#include "value_format.h"
#include <math.h>

namespace
{
  constexpr uint8_t  SH1107_ADDR   = 0x3C;
  constexpr uint16_t SH1107_WIDTH  = 128;
  constexpr uint16_t SH1107_HEIGHT = 128;
  constexpr uint8_t  lineHeight    = 8; // please adjust if you change font
}

DisplayManager::DisplayManager()
    : m_display(SH1107_HEIGHT, SH1107_WIDTH, &Wire, -1)
    , m_ready(false)
    , m_lastEnergyWs(NAN)
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
    return;

  m_display.clearDisplay();
  m_display.setCursor(0, lineHeight*3);
  m_display.println(F("connecting to"));
  
  // print ssid in font size 2 if possible
  int16_t x1, y1; uint16_t w, h; 
  m_display.setTextSize(2);
  m_display.getTextBounds(ssid, 0, 0, &x1, &y1, &w, &h);

  if (w > m_display.width())
    m_display.setTextSize(1);

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
    m_display.setTextSize(1);
    m_display.println(F("INA228 error"));
  }
  else
  {
    // last current and energy measurement
    m_display.setTextSize(1);
    m_display.println("Current");
    m_display.setCursor(0, m_display.getCursorY() + lineHeight / 2);

    const String currentStr = formatValue(values.current_mA / 1000.0f, "A", 5);
    float deltaWh = max((!isnan(m_lastEnergyWs)) ? (values.energyWs - m_lastEnergyWs) / 3600.0f : 0.0f, 0.0f);
    float totalWh = values.energyWs / 3600.0f;

    const String deltaEnergyStr = formatValue(deltaWh, "Wh", 5);
    const String energyStr = formatValue(totalWh, "Wh", 5);

    m_display.setTextSize(2);
    m_display.println(currentStr);
    m_display.println(deltaEnergyStr);

    // total accumulated energy
    m_display.setCursor(0, m_display.getCursorY() + lineHeight * 2);
    m_display.setTextSize(1);
    m_display.println("Total");
    m_display.setCursor(0, m_display.getCursorY() + lineHeight / 2);

    m_display.setTextSize(2);
    m_display.println(energyStr);
    m_display.println("");

    // vbus, die temp and IP address on botton
    m_display.setCursor(0, SH1107_HEIGHT - 3 * lineHeight);
    m_display.setTextSize(1);
    m_display.print(F("Vbus: "));
    m_display.println(formatValue(values.vBus, "V", 3));
    m_display.print(F("Temp: "));
    m_display.print(values.temperature, 1);
    m_display.println(F(" C"));

    m_lastEnergyWs = values.energyWs;
  }

  m_display.setCursor(0, SH1107_HEIGHT - lineHeight);
  m_display.setTextSize(1);
  m_display.print(F("IP:   "));
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
