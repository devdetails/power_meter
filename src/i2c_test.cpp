#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_INA228.h>

constexpr int SDA_PIN = D2;
constexpr int SCL_PIN = D1;

constexpr uint8_t INA228_ADDR       = 0x40;   // A0 = GND
constexpr float   INA228_SHUNT_OHMS = 0.33;   // shunt resistance

constexpr uint8_t  SH1107_ADDR   = 0x3C;
constexpr uint16_t SH1107_WIDTH  = 128;
constexpr uint16_t SH1107_HEIGHT = 128;

void scanI2C()
{
  byte error;
  int nDevices = 0;

  for (uint8_t address = 1; address < 127; address++) 
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) 
    {
      Serial.print(F("I2C device found at 0x"));
      if (address < 16) Serial.print('0');
      Serial.println(address, HEX);
      nDevices++;
    }
  }

  if (nDevices == 0) Serial.println(F("No I2C devices found"));
  else               Serial.println(F("Scan complete"));
  Serial.println();
}

Adafruit_SH1107 display(SH1107_HEIGHT, SH1107_WIDTH, &Wire, -1);
bool displayReady = false;

Adafruit_INA228 ina228;
bool inaReady = false;

struct InaValues 
{
  float vShunt;
  float vBus;
  float temperature;
  float current_mA;
};

bool readInaValues(InaValues &values) 
{
  if (!inaReady) 
    return false;

  values.vShunt      = ina228.readShuntVoltage() / 1000.0f;
  values.vBus        = ina228.readBusVoltage();
  values.temperature = ina228.readDieTemp();
  values.current_mA  = ina228.getCurrent_mA();

  return true;
}

void showMeasurements(const InaValues &values, bool ok) 
{
  if (!displayReady) 
    return;

  display.clearDisplay();
  display.setCursor(0, 0);

  if (!ok) 
  {
    display.println(F("INA228 error"));
  } 
  else 
  {
    display.println(F("INA228 values"));
    display.print(F("Vbus : "));
    display.print(values.vBus, 2);
    display.println(F(" V"));
    display.print(F("Vshunt: "));
    display.print(values.vShunt, 4);
    display.println(F(" V"));
    display.print(F("Temp : "));
    display.print(values.temperature, 1);
    display.println(F(" C"));
    display.print(F("Ishunt: "));
    display.print(values.current_mA, 2);
    display.println(F(" mA"));
  }

  display.display();
}

void setup() 
{
  Serial.begin(115200);
  delay(50);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  Serial.println(F("\nI2C scanner started..."));
  scanI2C();

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
    Serial.println(F("INA228 init OK"));
    inaReady = true;
  }

  if (!display.begin(SH1107_ADDR, false)) 
  {
    Serial.println(F("SH1107 OLED could not be initialized."));
  } 
  else 
  {
    display.clearDisplay();
    display.setRotation(3); // 90 deg counter-clockwise
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("SH1107 init OK"));
    display.display();
    displayReady = true;
  }
}


void loop() 
{  
  static unsigned long lastMeasurement = 0;
  if (millis() - lastMeasurement < 500) 
    return;

  lastMeasurement = millis();

  InaValues values{};
  bool ok = readInaValues(values);

  if (ok) 
  {
    Serial.printf("Vbus=%.3f V, Vshunt=%.6f V, Temp=%.2f C, I=%.2f mA\n",
                  values.vBus, values.vShunt, values.temperature,
                  values.current_mA);
  } else 
  {
    Serial.println(F("Error reading INA228"));
  }

  showMeasurements(values, ok);
}
