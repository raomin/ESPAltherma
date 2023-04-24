#include "arrayFunctions.hpp"

bool contains(uint8_t *array, size_t size, uint8_t value)
{
  for (size_t i = 0; i < size; i++) {
    if (array[i] == value) {
      return true;
    }
  }
  return false;
}