#pragma once

#include <stddef.h>

class MeasurementHistory
{
public:
  static constexpr size_t kCapacity = 64;

  MeasurementHistory()
      : m_count(0)
      , m_head(0)
  {
    for (size_t i = 0; i < kCapacity; ++i)
    {
      m_current[i]   = 0.0f;
      m_energy[i]    = 0.0f;
      m_timestamp[i] = 0.0f;
    }
  }

  void addMeasurement(float current_mA, float energyWs, float timestampSeconds)
  {
    m_current[m_head]   = current_mA;
    m_energy[m_head]    = energyWs;
    m_timestamp[m_head] = timestampSeconds;

    m_head = (m_head + 1) % kCapacity;
    if (m_count < kCapacity)
    {
      ++m_count;
    }
  }

  size_t copyCurrents(float *dest, size_t maxCount) const
  {
    return copyBuffer(m_current, dest, maxCount);
  }

  size_t copyEnergy(float *dest, size_t maxCount) const
  {
    return copyBuffer(m_energy, dest, maxCount);
  }

  size_t copyTimestamps(float *dest, size_t maxCount) const
  {
    return copyBuffer(m_timestamp, dest, maxCount);
  }

  size_t count() const
  {
    return m_count;
  }

  struct CurrentStats
  {
    float minCurrent;
    float maxCurrent;
    float meanCurrent;
    float stdDeviation;
  };

  CurrentStats getCurrentStats() const
  {
    CurrentStats stats = {0.0f, 0.0f, 0.0f, 0.0f};

    if (m_count == 0)
      return stats;

    // Single pass: compute min, max, sum, and sum of squares
    float minVal = m_current[0];
    float maxVal = m_current[0];
    float sum    = m_current[0];
    float sumSq  = m_current[0] * m_current[0];

    for (size_t i = 1; i < m_count; ++i)
    {
      const float val = m_current[i];

      if (val < minVal) minVal = val;
      if (val > maxVal) maxVal = val;

      sum   += val;
      sumSq += val * val;
    }

    stats.minCurrent  = minVal;
    stats.maxCurrent  = maxVal;
    stats.meanCurrent = sum / static_cast<float>(m_count);

    if (m_count >= 2)
    {
      const float variance = (sumSq / static_cast<float>(m_count)) - (stats.meanCurrent * stats.meanCurrent);
      stats.stdDeviation   = sqrt(variance > 0.0f ? variance : 0.0f);
    }

    return stats;
  }

private:
  size_t copyBuffer(const float *src, float *dest, size_t maxCount) const
  {
    const size_t toCopy = (m_count < maxCount) ? m_count : maxCount;

    for (size_t i = 0; i < toCopy; ++i)
    {
      const size_t index = (m_head + kCapacity - toCopy + i) % kCapacity;
      dest[i]            = src[index];
    }

    return toCopy;
  }

  float  m_current[kCapacity];
  float  m_energy[kCapacity];
  float  m_timestamp[kCapacity];
  size_t m_count;
  size_t m_head;
};
