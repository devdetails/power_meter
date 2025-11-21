#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA228.h>

#include "secrets.h"
#include "ina_values.h"
#include "value_format.h"
#include "display_manager.h"
#include "measurement_history.h"
#include "webinterface.h"

constexpr int SDA_PIN = D2;
constexpr int SCL_PIN = D1;

constexpr uint8_t INA228_ADDR       = 0x40;   // A0 = GND
constexpr float   INA228_SHUNT_OHMS = 0.33;   // shunt resistance
constexpr uint8_t BUTTON_PIN        = 0;      // GPIO0

constexpr unsigned long MEASUREMENT_INTERVAL_MS = 500;
constexpr unsigned long BUTTON_DEBOUNCE_MS      = 50;

// 0.05 Ohm shunt
// Resolution: LSB=6.25uA  for ADCRANGE=0 and LSB=1.56uA  for ADCRANGE=1
// Saturates:  MAX=3.2768A for ADCRANGE=0 and MAX=0.8192A for ADCRANGE=1


Adafruit_INA228    ina228;
bool               inaReady = false;

WebInterface       webInterface;
IPAddress          webIp;
bool               webConnected = false;
DisplayManager     displayManager;
MeasurementHistory measurementHistory;
DisplayMode        displayMode = DisplayMode::Summary;

void handleButton()
{
  static bool           lastReading     = HIGH;
  static bool           stableState     = HIGH;
  static unsigned long  lastChangeTime  = 0;

  const bool            reading         = digitalRead(BUTTON_PIN);
  const unsigned long   now             = millis();

  if (reading != lastReading)
  {
    lastChangeTime = now;
  }

  if ((now - lastChangeTime) > BUTTON_DEBOUNCE_MS && reading != stableState)
  {
    stableState = reading;
    if (stableState == LOW)
    {
      switch (displayMode)
      {
        case DisplayMode::Summary:
          displayMode = DisplayMode::GraphCurrent;
          break;
        case DisplayMode::GraphCurrent:
          displayMode = DisplayMode::GraphEnergy;
          break;
        case DisplayMode::GraphEnergy:
          displayMode = DisplayMode::Summary;
          break;
      }
    }
  }

  lastReading = reading;
}

bool readInaValues(InaValues &values)
{
  if (!inaReady)
  {
    return false;
  }

  values.vShunt      = ina228.readShuntVoltage() / 1000.0f;
  values.vBus        = ina228.readBusVoltage();
  values.temperature = ina228.readDieTemp();
  values.current_mA  = ina228.getCurrent_mA();
  values.energyWs    = ina228.readEnergy();

  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(50);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!ina228.begin(INA228_ADDR, &Wire))
  {
    Serial.println(F("INA228 could not be initialized."));
  }
  else
  {
    ina228.setADCRange(0);
    ina228.setAveragingCount(INA228_COUNT_128);
    ina228.setCurrentConversionTime(INA228_TIME_1052_us);
    ina228.setVoltageConversionTime(INA228_TIME_1052_us);
    ina228.setTemperatureConversionTime(INA228_TIME_1052_us);
    ina228.setMode(INA228_MODE_CONTINUOUS);
    ina228.setShunt(INA228_SHUNT_OHMS);
    ina228.resetAccumulators();
    Serial.println(F("INA228 init OK"));
    inaReady = true;
  }

  displayManager.begin();
  displayManager.showConnecting(secrets::WIFI_SSID);

  webConnected = webInterface.begin(secrets::WIFI_SSID, secrets::WIFI_PASSWORD);
  webIp        = webInterface.localIp();
}


void loop()
{
  handleButton();

  static unsigned long lastMeasurement = 0;
  const unsigned long  now             = millis();

  if (now - lastMeasurement >= MEASUREMENT_INTERVAL_MS)
  {
    lastMeasurement = now;

    InaValues values{};
    bool      ok = readInaValues(values);

    if (ok)
    {
      measurementHistory.addMeasurement(values.current_mA, values.energyWs,
                                        static_cast<float>(millis()) / 1000.0f);
      Serial.printf("Vbus=%s Vshunt=%s Temp=%.2f C I=%s E=%s\n",
                    formatValue(values.vBus, "V", 5).c_str(),
                    formatValue(values.vShunt, "V", 5).c_str(),
                    values.temperature,
                    formatValue(values.current_mA / 1000.0f, "A", 5).c_str(),
                    formatValue(values.energyWs / 3600.0f, "Wh", 5).c_str());
      webInterface.updateMeasurements(values);
    }
    else
    {
      Serial.println(F("Error reading INA228"));
    }

    webConnected = webInterface.isConnected();
    webIp        = webInterface.localIp();
    displayManager.showMeasurements(values, ok, webConnected, webIp, measurementHistory, displayMode);
  }

  webInterface.loop();
}
