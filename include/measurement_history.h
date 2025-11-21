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
