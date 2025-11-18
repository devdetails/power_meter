#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA228.h>

#include "secrets.h"
#include "ina_values.h"
#include "display_manager.h"
#include "webinterface.h"

constexpr int SDA_PIN = D2;
constexpr int SCL_PIN = D1;

constexpr uint8_t INA228_ADDR       = 0x40;   // A0 = GND
constexpr float   INA228_SHUNT_OHMS = 0.33;   // shunt resistance

Adafruit_INA228 ina228;
bool            inaReady = false;

WebInterface   webInterface;
IPAddress      webIp;
bool           webConnected = false;
DisplayManager displayManager;

bool readInaValues(InaValues &values)
{
  if (!inaReady)
  {
    return false;
  }

  values.vShunt      = ina228.readShuntVoltage();
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

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!ina228.begin(INA228_ADDR, &Wire))
  {
    Serial.println(F("INA228 could not be initialized."));
  }
  else
  {
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
  static unsigned long lastMeasurement = 0;
  if (millis() - lastMeasurement < 500)
    return;

  lastMeasurement = millis();

  InaValues values{};
  bool      ok = readInaValues(values);

  if (ok)
  {
    Serial.printf("Vbus=%.3f V, Vshunt=%.6f V, Temp=%.2f C, I=%.2f mA, E=%.3f W*s\n",
                  values.vBus, values.vShunt, values.temperature,
                  values.current_mA, values.energyWs);
    webInterface.updateMeasurements(values);
  }
  else
  {
    Serial.println(F("Error reading INA228"));
  }

  webConnected = webInterface.isConnected();
  webIp        = webInterface.localIp();
  displayManager.showMeasurements(values, ok, webConnected, webIp);

  webInterface.loop();
}
