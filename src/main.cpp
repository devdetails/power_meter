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
constexpr float   INA228_SHUNT_OHMS = 0.015;  // shunt resistance
constexpr uint8_t BUTTON_PIN        = 0;      // GPIO0
constexpr uint8_t INA_ALERT_PIN     = D5;     // GPIO14

constexpr unsigned long UPDATE_INTERVAL_MS = 500;
constexpr unsigned long BUTTON_DEBOUNCE_MS      = 50;

// 0.05 Ohm shunt
// Resolution: LSB=6.25uA  for ADCRANGE=0 and LSB=1.56uA  for ADCRANGE=1
// Saturates:  MAX=3.2768A for ADCRANGE=0 and MAX=0.8192A for ADCRANGE=1


Adafruit_INA228    ina228;
bool               inaReady = false;

volatile uint32_t  inaAlertCount      = 0;
InaValues          lastMeasuredValues = {};
bool               lastMeasurementOk  = false;

WebInterface       webInterface;
IPAddress          webIp;
bool               webConnected = false;
DisplayManager     displayManager;
MeasurementHistory measurementHistory;
DisplayMode        displayMode = DisplayMode::Summary;

struct MeasurementAccumulator
{
  float     vShuntSum      = 0.0f;
  float     vBusSum        = 0.0f;
  float     temperatureSum = 0.0f;
  float     currentSum     = 0.0f;
  float     totalEnergyWs  = 0.0f;
  uint32_t  count          = 0;

  void add(const InaValues &values)
  {
    vShuntSum      += values.vShunt;
    vBusSum        += values.vBus;
    temperatureSum += values.temperature;
    currentSum     += values.current_mA;
    totalEnergyWs   = values.energyWs;

    ++count;
  }

  void reset()
  {
    vShuntSum      = 0.0f;
    vBusSum        = 0.0f;
    temperatureSum = 0.0f;
    currentSum     = 0.0f;
    totalEnergyWs  = 0.0f;
    count          = 0;
  }

  bool hasData() const
  {
    return count > 0;
  }

  InaValues average() const
  {
    InaValues averaged{};

    if (count > 0)
    {
      const float invCount = 1.0f / static_cast<float>(count);
      averaged.vShunt      = vShuntSum      * invCount;
      averaged.vBus        = vBusSum        * invCount;
      averaged.temperature = temperatureSum * invCount;
      averaged.current_mA  = currentSum     * invCount;
      averaged.energyWs    = totalEnergyWs; // keep the latest accumulated energy
    }

    return averaged;
  }
};

MeasurementAccumulator measurementAccumulator;

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
  if (readInaValues(values))
  {
    measurementAccumulator.add(values);
  }

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
    ina228.setADCRange(0);
    ina228.setAveragingCount(INA228_COUNT_64);
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

  static unsigned long lastUpdate   = 0;
  static float         lastEnergyWs = lastMeasuredValues.energyWs;
  const unsigned long  now          = millis();

  if (now - lastUpdate >= UPDATE_INTERVAL_MS)
  {
    uint32_t sampleCount = measurementAccumulator.count;

    if (measurementAccumulator.hasData())
    {
      float prevEnergyWs = lastMeasuredValues.energyWs;
      lastMeasuredValues = measurementAccumulator.average();

      lastEnergyWs = lastMeasuredValues.energyWs - prevEnergyWs;
      measurementHistory.addMeasurement(lastMeasuredValues.current_mA, lastMeasuredValues.energyWs, static_cast<float>(now) / 1000.0f);

      measurementAccumulator.reset();
      lastMeasurementOk = true;

      // Get fluctuation metrics from measurement history
      size_t historyCount = measurementHistory.count();
      if (historyCount >= 2)
      {
        MeasurementHistory::CurrentStats stats = measurementHistory.getCurrentStats();
        float fluctuation = stats.maxCurrent - stats.minCurrent;
        float stdDevPercent = (stats.meanCurrent > 0.0f) ? (stats.stdDeviation / stats.meanCurrent) * 100.0f : 0.0f;
        float rangePercent = (stats.meanCurrent > 0.0f) ? (fluctuation / stats.meanCurrent) * 100.0f : 0.0f;

        Serial.printf("[avg %lu] Vbus=%s Vshunt=%s Temp=%.2f C I=%s E=%s | I-stddev=%.1f%% I-range=%.1f%%\n",
                      static_cast<unsigned long>(sampleCount),
                      formatValue(lastMeasuredValues.vBus,   "V", 5).c_str(),
                      formatValue(lastMeasuredValues.vShunt, "V", 5).c_str(),
                      lastMeasuredValues.temperature,
                      formatValue(lastMeasuredValues.current_mA / 1000.0f, "A", 5).c_str(),
                      formatValue(lastMeasuredValues.energyWs / 3600.0f, "Wh", 5).c_str(),
                      stdDevPercent,
                      rangePercent);
      }
      else
      {
        Serial.printf("[avg %lu] Vbus=%s Vshunt=%s Temp=%.2f C I=%s E=%s (insufficient history for fluctuation)\n",
                      static_cast<unsigned long>(sampleCount),
                      formatValue(lastMeasuredValues.vBus,   "V", 5).c_str(),
                      formatValue(lastMeasuredValues.vShunt, "V", 5).c_str(),
                      lastMeasuredValues.temperature,
                      formatValue(lastMeasuredValues.current_mA / 1000.0f, "A", 5).c_str(),
                      formatValue(lastMeasuredValues.energyWs / 3600.0f, "Wh", 5).c_str());
      }
    }
    else
    {
      Serial.printf("[avg %lu] No measurements in this cycle\n", static_cast<unsigned long>(sampleCount));
    }

    webInterface.updateMeasurements(lastMeasuredValues);
    displayManager.showMeasurements(lastMeasuredValues, lastEnergyWs, lastMeasurementOk, webConnected, webIp, measurementHistory, displayMode);
    
   
    if (!lastMeasurementOk)
    {                       
      Serial.println(F("Error reading INA228"));
    }

    webConnected = webInterface.isConnected();
    webIp        = webInterface.localIp();

    lastUpdate = now;
  }

  webInterface.loop();
}
