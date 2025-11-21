#include "display_manager.h"

#include "ina_values.h"
#include "measurement_history.h"
#include "value_format.h"
#include "util/small_sort.h"
#include <math.h>
#include <string.h>

namespace
{
  constexpr uint8_t  SH1107_ADDR          = 0x3C;
  constexpr uint16_t SH1107_WIDTH         = 128;
  constexpr uint16_t SH1107_HEIGHT        = 128;
  constexpr uint8_t  lineHeight           = 8; // please adjust if you change font
  constexpr int16_t  graphMarginLeft      = 35;
  constexpr int16_t  graphMarginRight     = 6;
  constexpr int16_t  graphMarginTop       = 18;
  constexpr int16_t  graphMarginBottom    = 34;
  constexpr uint8_t  yTickCount           = 4;
  constexpr uint8_t  xTickCount           = 1; // produces 2 ticks (0 and 1)
  constexpr float    graphSmoothingAlpha  = 0.2f;
  constexpr float    minGraphRange        = 0.0001f;
  constexpr float    graphPaddingFraction = 0.1f;
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
    Serial.println(F("SH1107 OLED could not be initialized."));
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

void DisplayManager::showMeasurements(const InaValues &values, bool sensorOk, bool webConnected, const IPAddress &ip,
                                      const MeasurementHistory &history, DisplayMode mode)
{
  if (!m_ready)
  {
    return;
  }

  m_display.clearDisplay();

  if (!sensorOk)
  {
    m_display.setCursor(0, lineHeight * 2);
    m_display.setTextSize(1);
    m_display.println(F("INA228 error"));
  }
  else if (mode == DisplayMode::Summary)
  {
    m_display.setCursor(0, 0);
    // last current and energy measurement
    m_display.setTextSize(1);
    m_display.println(F("Current"));
    m_display.setCursor(0, m_display.getCursorY() + lineHeight / 2);

    const String currentStr = formatValue(values.current_mA / 1000.0f, "A", 5);
    float        deltaWs    = max((!isnan(m_lastEnergyWs)) ? values.energyWs - m_lastEnergyWs : 0.0f, 0.0f);

    const String intervalEnergyStr = formatValue(deltaWs         / 3600.0f, "Wh", 5);
    const String totalEnergyStr    = formatValue(values.energyWs / 3600.0f, "Wh", 5);

    m_display.setTextSize(2);
    m_display.println(currentStr);
    m_display.println(intervalEnergyStr);

    // total accumulated energy
    m_display.setCursor(0, m_display.getCursorY() + lineHeight * 2);
    m_display.setTextSize(1);
    m_display.println(F("Total"));
    m_display.setCursor(0, m_display.getCursorY() + lineHeight / 2);

    m_display.setTextSize(2);
    m_display.println(totalEnergyStr);
    m_display.println();

    // vbus, die temp and IP address on botton
    m_display.setCursor(0, SH1107_HEIGHT - 3 * lineHeight);
    m_display.setTextSize(1);
    m_display.print(F("Vbus: "));
    m_display.println(formatValue(values.vBus, "V", 4));
    m_display.print(F("Temp: "));
    m_display.print(values.temperature, 1);
    m_display.println(F(" C"));

    m_lastEnergyWs = values.energyWs;
  }
  else
  {
    showGraph(history, mode);
    m_lastEnergyWs = values.energyWs;
  }

  if (mode == DisplayMode::Summary)
  {
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
  }

  m_display.display();
}

void DisplayManager::showGraph(const MeasurementHistory &history, DisplayMode mode)
{
  float      samples[MeasurementHistory::kCapacity];
  float      timestamps[MeasurementHistory::kCapacity];
  const bool showCurrent = (mode == DisplayMode::GraphCurrent);
  const char *unit       = showCurrent ? "A" : "Wh";

  size_t count = showCurrent ? history.copyCurrents(samples, MeasurementHistory::kCapacity)
                             : history.copyEnergy(samples, MeasurementHistory::kCapacity);
  const size_t tsCount = history.copyTimestamps(timestamps, MeasurementHistory::kCapacity);
  count                = min(count, tsCount);

  m_display.setCursor(0, 0);
  m_display.setTextSize(1);

  if (count < 2)
  {
    m_display.println(F("Graph: waiting"));
    m_display.setCursor(0, SH1107_HEIGHT / 2);
    m_display.println(F("Waiting for data"));
    return;
  }

  float values[MeasurementHistory::kCapacity];
  const float conversion = showCurrent ? 0.001f : (1.0f / 3600.0f);
  for (size_t i = 0; i < count; ++i)
  {
    values[i] = samples[i] * conversion;
  }

  GraphScaleState &state = showCurrent ? m_currentScale : m_energyScale;
  updateScaleWithHistory(state, values, count);

  float minVal = min(state.min, state.stickyMin);
  float maxVal = max(state.max, state.stickyMax);
  if (fabs(maxVal - minVal) < minGraphRange)
  {
    const float padding = minGraphRange * 0.5f;
    minVal -= padding;
    maxVal += padding;
  }

  float range    = maxVal - minVal;
  const float pad = max(range * graphPaddingFraction, minGraphRange);
  minVal -= pad;
  maxVal += pad;
  range = maxVal - minVal;

  const int16_t graphWidth  = SH1107_WIDTH - graphMarginLeft - graphMarginRight;
  const int16_t graphHeight = SH1107_HEIGHT - graphMarginTop - graphMarginBottom;
  const int16_t originX     = graphMarginLeft;
  const int16_t originY     = graphMarginTop + graphHeight;

  const float maxAbsValue = max(fabs(minVal), fabs(maxVal));
  String      unitLabel(unit);

  if (maxAbsValue > 0.0f)
  {
    const ValuePrefix prefix = findValuePrefix(maxAbsValue);
    unitLabel = prefix.symbol;
    unitLabel += unit;
    if (unitLabel.length() == 0)
    {
      unitLabel = unit;
    }
  }

  String title = showCurrent ? "Current (" : "Energy (";
  title += unitLabel;
  title += ")";

  int16_t titleX = SH1107_WIDTH - (title.length() * 6);

  if (titleX < 0)
  {
    titleX = 0;
  }

  m_display.setCursor(titleX, 0);
  m_display.println(title);

  // axes
  m_display.drawLine(originX, originY, originX + graphWidth, originY, SH110X_WHITE);
  m_display.drawLine(originX, originY, originX, originY - graphHeight, SH110X_WHITE);

  // y-axis ticks and labels
  for (uint8_t i = 0; i <= yTickCount; ++i)
  {
    const float   position = static_cast<float>(i) / yTickCount;
    const int16_t y        = originY - static_cast<int16_t>(round(position * graphHeight));
    const float   value    = minVal + (range * position);

    m_display.drawLine(originX - 3, y, originX, y, SH110X_WHITE);

    const String label = formatValue(value, nullptr, 4);
    int16_t      textY = y - lineHeight / 2;

    if (textY < 0)
    {
      textY = 0;
    }
    
    m_display.setCursor(2, textY);
    m_display.print(label);
  }

  const float yScale = (range > 0.0f) ? (static_cast<float>(graphHeight) / range) : 0.0f;

  const float startTime = timestamps[0];
  const float endTime   = timestamps[count - 1];
  const float duration  = max(endTime - startTime, 0.0001f);

  int16_t prevX = originX;
  int16_t prevY = originY - static_cast<int16_t>(round((values[0] - minVal) * yScale));
  prevY         = constrain(prevY, originY - graphHeight, originY);

  for (size_t i = 1; i < count; ++i)
  {
    float relative = (duration > 0.0f) ? ((timestamps[i] - startTime) / duration)
                                       : (static_cast<float>(i) / (count - 1));
    relative       = constrain(relative, 0.0f, 1.0f);

    int16_t x = originX + static_cast<int16_t>(round(relative * graphWidth));
    int16_t y = originY - static_cast<int16_t>(round((values[i] - minVal) * yScale));

    x = constrain(x, originX, originX + graphWidth);
    y = constrain(y, originY - graphHeight, originY);

    m_display.drawLine(prevX, prevY, x, y, SH110X_WHITE);
    prevX = x;
    prevY = y;
  }

  // x-axis ticks and labels
  for (uint8_t i = 0; i <= xTickCount; ++i)
  {
    const float   position = static_cast<float>(i) / xTickCount;
    const int16_t x        = originX + static_cast<int16_t>(round(position * graphWidth));
    const float   seconds  = startTime + (duration * position);

    m_display.drawLine(x, originY, x, originY + 3, SH110X_WHITE);

    const String label      = formatTime(seconds);
    const int16_t textWidth = label.length() * 6;
    int16_t      textX      = x - textWidth / 2;
    if (textX < 0)
    {
      textX = 0;
    }
    if (textX + textWidth > SH1107_WIDTH)
    {
      textX = SH1107_WIDTH - textWidth;
    }

    m_display.setCursor(textX, originY + lineHeight);
    m_display.print(label);
  }

  m_display.setCursor(originX + graphWidth - 4 * 6, originY + (lineHeight * 2));
  m_display.print(F("Time"));
}

void DisplayManager::updateScaleWithHistory(GraphScaleState &state, const float *values, size_t count)
{
  if (count == 0)
  {
    return;
  }

  float sorted[MeasurementHistory::kCapacity];
  memcpy(sorted, values, count * sizeof(float));
  insertionSort(sorted, count);

  const size_t lowIndex  = static_cast<size_t>(roundf(0.05f * (count - 1)));
  const size_t highIndex = static_cast<size_t>(roundf(0.95f * (count - 1)));
  const float  rawMin    = sorted[min(lowIndex, count - 1)];
  const float  rawMax    = sorted[min(highIndex, count - 1)];

  constexpr uint8_t stickyHoldFrames = 30;

  if (!state.initialized)
  {
    state.min         = rawMin;
    state.max         = rawMax;
    state.stickyMin   = rawMin;
    state.stickyMax   = rawMax;
    state.holdFrames  = stickyHoldFrames;
    state.initialized = true;
    return;
  }

  state.min += (rawMin - state.min) * graphSmoothingAlpha;
  state.max += (rawMax - state.max) * graphSmoothingAlpha;

  if (rawMin < state.stickyMin || state.holdFrames == 0)
  {
    state.stickyMin  = rawMin;
    state.holdFrames = stickyHoldFrames;
  }
  else if (rawMin > state.stickyMin)
  {
    state.stickyMin += (rawMin - state.stickyMin) * (graphSmoothingAlpha * 0.5f);
  }

  if (rawMax > state.stickyMax || state.holdFrames == 0)
  {
    state.stickyMax  = rawMax;
    state.holdFrames = stickyHoldFrames;
  }
  else if (rawMax < state.stickyMax)
  {
    state.stickyMax += (rawMax - state.stickyMax) * (graphSmoothingAlpha * 0.5f);
  }

  if (state.holdFrames > 0)
  {
    --state.holdFrames;
  }
}
