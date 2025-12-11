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
constexpr float   INA228_SHUNT_OHMS = 0.05;   // shunt resistance
constexpr uint8_t BUTTON_PIN        = 0;      // GPIO0
constexpr uint8_t INA_ALERT_PIN     = D5;     // GPIO14

constexpr unsigned long BUTTON_DEBOUNCE_MS      = 50;
constexpr unsigned long WEB_LOOP_INTERVAL_MS    = 10;

// 0.05 Ohm shunt
// Resolution: LSB=6.25uA  for ADCRANGE=0 and LSB=1.56uA  for ADCRANGE=1
// Saturates:  MAX=3.2768A for ADCRANGE=0 and MAX=0.8192A for ADCRANGE=1


Adafruit_INA228    ina228;
bool               inaReady = false;

volatile uint32_t  inaAlertCount      = 0;
InaValues          lastMeasuredValues = {};
bool               lastMeasurementOk  = false;
float              lastEnergyDeltaWs  = 0.0f;

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

void IRAM_ATTR onInaAlert()
{
  ++inaAlertCount;
}

void processInaAlerts()
{
  if (!inaReady)
  {
    return;
  }

  uint32_t alerts = 0;
  noInterrupts();
  alerts        = inaAlertCount;
  inaAlertCount = 0;
  interrupts();

  if (alerts == 0)
  {
    return;
  }

  // INA228 doesn't buffer multiple conversions, so one read gets latest data.
  InaValues values{};
  if (!readInaValues(values))
  {
    lastMeasurementOk = false;
    Serial.println(F("Error reading INA228"));
    ina228.alertFunctionFlags();
    return;
  }

  const unsigned long nowMs          = millis();
  const float         prevEnergyWs   = lastMeasuredValues.energyWs;
  lastMeasuredValues                 = values;
  lastEnergyDeltaWs                  = max(lastMeasuredValues.energyWs - prevEnergyWs, 0.0f);
  lastMeasurementOk                  = true;

  measurementHistory.addMeasurement(lastMeasuredValues.current_mA, lastMeasuredValues.energyWs, static_cast<float>(nowMs) / 1000.0f);

  size_t historyCount = measurementHistory.count();
  if (historyCount >= 2)
  {
    MeasurementHistory::CurrentStats stats = measurementHistory.getCurrentStats();
    const float fluctuation_mA = stats.maxCurrent - stats.minCurrent; // mA
    const float stdDev_mA      = stats.stdDeviation; // mA
    const float mean_mA        = stats.meanCurrent; // mA

    const float stdDevPercent = (mean_mA > 0.0f) ? (stdDev_mA / mean_mA) * 100.0f : 0.0f;
    const float rangePercent  = (mean_mA > 0.0f) ? (fluctuation_mA / mean_mA) * 100.0f : 0.0f;

    // formatValue expects base unit in base units (A), so convert mA -> A
    const String stdDevStr = formatValue(stdDev_mA / 1000.0f, "A", 5);
    const String rangeStr  = formatValue(fluctuation_mA / 1000.0f, "A", 5);

    Serial.printf("[meas %lu] Vbus=%s Vshunt=%s Temp=%.2f C I=%s E=%s | I-stddev=%s (%.3f%%) I-range=%s (%.3f%%)\n",
                  static_cast<unsigned long>(historyCount),
                  formatValue(lastMeasuredValues.vBus,   "V", 5).c_str(),
                  formatValue(lastMeasuredValues.vShunt, "V", 5).c_str(),
                  lastMeasuredValues.temperature,
                  formatValue(lastMeasuredValues.current_mA / 1000.0f, "A",  5).c_str(),
                  formatValue(lastMeasuredValues.energyWs   / 3600.0f, "Wh", 5).c_str(),
                  stdDevStr.c_str(),
                  stdDevPercent,
                  rangeStr.c_str(),
                  rangePercent);

    // Output in plotter-friendly format (CSV)
    Serial.printf(">I_avg:%.2f\n", mean_mA);
    Serial.printf(">I_stddev:%.4f\n", stdDev_mA);
  }
  else
  {
    Serial.printf("[meas %lu] Vbus=%s Vshunt=%s Temp=%.2f C I=%s E=%s (insufficient history for fluctuation)\n",
                  static_cast<unsigned long>(historyCount),
                  formatValue(lastMeasuredValues.vBus,   "V", 5).c_str(),
                  formatValue(lastMeasuredValues.vShunt, "V", 5).c_str(),
                  lastMeasuredValues.temperature,
                  formatValue(lastMeasuredValues.current_mA / 1000.0f, "A", 5).c_str(),
                  formatValue(lastMeasuredValues.energyWs / 3600.0f, "Wh", 5).c_str());
  }

  webInterface.updateMeasurements(lastMeasuredValues);
  displayManager.showMeasurements(lastMeasuredValues, lastEnergyDeltaWs, lastMeasurementOk, webConnected, webIp, measurementHistory, displayMode);

  // Reading alert flags clears the CONV_READY alert so it can fire again.
  ina228.alertFunctionFlags();
}

void setup()
{
  Serial.begin(115200);
  delay(50);

  pinMode(BUTTON_PIN,    INPUT_PULLUP);
  pinMode(INA_ALERT_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!ina228.begin(INA228_ADDR, &Wire))
  {
    Serial.println(F("INA228 could not be initialized."));
  }
  else
  {
    ina228.setADCRange(1);
    ina228.setAveragingCount(INA228_COUNT_256);
    ina228.setCurrentConversionTime(INA228_TIME_4120_us);
    ina228.setVoltageConversionTime(INA228_TIME_4120_us);
    ina228.setTemperatureConversionTime(INA228_TIME_1052_us);
    ina228.setMode(INA228_MODE_CONTINUOUS);
    ina228.setAlertPolarity(INA228_ALERT_POLARITY_INVERTED);
    ina228.setAlertLatch(INA228_ALERT_LATCH_TRANSPARENT);
    ina228.setAlertType(INA228_ALERT_CONVERSION_READY);
    ina228.setShunt(INA228_SHUNT_OHMS);
    ina228.resetAccumulators();
    Serial.println(F("INA228 init OK"));
    inaReady = true;

    attachInterrupt(digitalPinToInterrupt(INA_ALERT_PIN), onInaAlert, FALLING);
  }

  displayManager.begin();
  displayManager.showConnecting(secrets::WIFI_SSID);

  webConnected = webInterface.begin(secrets::WIFI_SSID, secrets::WIFI_PASSWORD);
  webIp        = webInterface.localIp();

  lastMeasurementOk = readInaValues(lastMeasuredValues);
}


void loop()
{
  handleButton();
  processInaAlerts();

  static unsigned long lastWebLoop = 0;
  const unsigned long  now         = millis();

  if (now - lastWebLoop >= WEB_LOOP_INTERVAL_MS)
  {
    webInterface.loop();
    webConnected = webInterface.isConnected();
    webIp        = webInterface.localIp();

    lastWebLoop = now;
  }
}
