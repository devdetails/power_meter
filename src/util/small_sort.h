#pragma once

#include <stddef.h>

inline void insertionSort(float *data, size_t count)
{
  for (size_t i = 1; i < count; ++i)
  {
    float key = data[i];
    size_t j  = i;
    while (j > 0 && data[j - 1] > key)
    {
      data[j] = data[j - 1];
      --j;
    }
    data[j] = key;
  }
}
