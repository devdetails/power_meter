#include "value_format.h"

#include <math.h>
#include <string.h>

namespace
{
  constexpr ValuePrefix kPrefixes[] = {
      { "n", 1e-9f },
      { "u", 1e-6f },
      { "m", 1e-3f },
      { "",  1.0f  },
      { "k", 1e3f  },
  };

  constexpr size_t kPrefixCount = sizeof(kPrefixes) / sizeof(kPrefixes[0]);
}

ValuePrefix findValuePrefix(float absValue)
{
  absValue = fabsf(absValue);

  size_t index = 0;
  while (index + 1 < kPrefixCount && absValue >= kPrefixes[index + 1].factor)
    ++index;

  return kPrefixes[index];
}

String formatValue(float baseValue, const char *baseUnit, uint8_t digits)
{
  digits = constrain(digits, 4, 10);

  const ValuePrefix prefix  = findValuePrefix(baseValue);

  const float scaled    = baseValue / prefix.factor;
  const float absScaled = fabsf(scaled);

  uint8_t decimals;
  if      (absScaled >= 100.0f) decimals = digits - 3;
  else if (absScaled >= 10.0f)  decimals = digits - 2;
  else                          decimals = digits - 1;
 
  char buffer[24];
  if (baseUnit != nullptr && baseUnit[0] != '\0')
  {
    // append prefix to baseUnit and pad to exactly 3 characters for alignment
    char       unit[4] = { 0 };
    snprintf(unit, sizeof(unit), "%s%s", prefix.symbol, baseUnit);

    size_t len = strlen(unit);
    while (len < 3 && len < sizeof(unit) - 1)
    {
      unit[len]     = ' ';
      unit[len + 1] = '\0';
      ++len;
    }

    snprintf(buffer, sizeof(buffer), "%.*f %s", decimals, scaled, unit);
  }
  else
  {
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, scaled);
  }

  return String(buffer);
}

String formatTime(float seconds)
{
  if (seconds < 0.0f)
  {
    seconds = 0.0f;
  }

  unsigned long totalSeconds = static_cast<unsigned long>(seconds + 0.5f);
  unsigned int  hours        = totalSeconds / 3600;
  unsigned int  minutes      = (totalSeconds % 3600) / 60;
  unsigned int  secs         = totalSeconds % 60;

  if (hours > 99)
  {
    hours   = 99;
    minutes = 59;
    secs    = 59;
  }

  char   chunk[8];
  String result;

  if (hours > 0)
  {
    snprintf(chunk, sizeof(chunk), "%02uh", hours);
    result += chunk;
  }
  if (minutes > 0)
  {
    snprintf(chunk, sizeof(chunk), "%02um", minutes);
    result += chunk;
  }

  snprintf(chunk, sizeof(chunk), "%02us", secs);
  result += chunk;

  return result;
}
