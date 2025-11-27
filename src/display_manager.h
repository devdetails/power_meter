#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <IPAddress.h>

struct InaValues;
class MeasurementHistory;

enum class DisplayMode
{
  Summary,
  GraphCurrent,
  GraphEnergy
};

class DisplayManager
{
public:
  DisplayManager();

  bool begin();
  void showConnecting(const char *ssid);
  void showMeasurements(const InaValues &values, float deltaEnergyWs, bool sensorOk, bool webConnected, const IPAddress &ip,
                        const MeasurementHistory &history, DisplayMode mode);

private:
  struct GraphScaleState
  {
    float min;
    float max;
    float stickyMin;
    float stickyMax;
    uint8_t holdFrames;
    bool    initialized;
    GraphScaleState() : min(0.0f), max(0.0f), stickyMin(0.0f), stickyMax(0.0f), holdFrames(0), initialized(false) {}
  };

  void        showGraph(const MeasurementHistory &history, DisplayMode mode);
  void        updateScaleWithHistory(GraphScaleState &state, const float *values, size_t count);

  Adafruit_SH1107 m_display;
  bool            m_ready;
  GraphScaleState m_currentScale;
  GraphScaleState m_energyScale;
};
