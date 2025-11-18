#include "value_format.h"

#include <math.h>
#include <string.h>

String formatValue(float baseValue, const char *baseUnit, uint8_t digits)
{
  digits = digits < 2 ? 2 : digits; // keep at least 1 integer digit and 1 decimal

  struct Prefix
  {
    const char *symbol;
    float       factor;
  };

  // Order allows scaling down (micro) and up (kilo) to fit into the requested digits + decimal.
  static constexpr Prefix prefixes[] = {
      { "u", 1e-6f },
      { "m", 1e-3f },
      { "",  1.0f  },
      { "k", 1e3f  },
  };

  size_t prefixIndex = 2; // start with base unit
  float  scaled      = baseValue;
  float  absValue    = fabsf(baseValue);

  const float upperLimit = powf(10.0f, digits - 1); // keep at least 1 decimal digit

  // Scale up for large numbers.
  while (fabsf(scaled) >= upperLimit && prefixIndex + 1 < (sizeof(prefixes) / sizeof(prefixes[0])))
  {
    ++prefixIndex;
    scaled = baseValue / prefixes[prefixIndex].factor;
  }

  // Scale down for very small numbers (but keep zero as-is).
  while (fabsf(scaled) < 1.0f && absValue > 0.0f && prefixIndex > 0)
  {
    --prefixIndex;
    scaled = baseValue / prefixes[prefixIndex].factor;
  }

  float absScaled = fabsf(scaled);
  if (absScaled >= upperLimit)
  {
    const float cap = upperLimit - 0.1f;
    scaled    = (scaled >= 0.0f) ? cap : -cap;
    absScaled = fabsf(scaled);
  }

  uint8_t intDigits = 1;
  if (absScaled >= 1.0f)
  {
    intDigits = static_cast<uint8_t>(floorf(log10f(absScaled))) + 1;
  }

  // Decide decimals based on the requested format ranges.
  uint8_t decimals = (intDigits >= digits) ? 1 : static_cast<uint8_t>(digits - intDigits);
  if (decimals == 0)
  {
    decimals = 1;
  }

  char unit[4] = { 0 };
  snprintf(unit, sizeof(unit), "%s%s", prefixes[prefixIndex].symbol, baseUnit);
  // Pad to exactly 3 characters to keep alignment.
  size_t len = strlen(unit);
  while (len < 3 && len < sizeof(unit) - 1)
  {
    unit[len] = ' ';
    unit[len + 1] = '\0';
    ++len;
  }

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%.*f %s", decimals, scaled, unit);
  return String(buffer);
}
