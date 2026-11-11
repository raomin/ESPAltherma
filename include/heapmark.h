#ifndef ESPALTHERMA_HEAPMARK_H
#define ESPALTHERMA_HEAPMARK_H

// The free data heap at named points of the start (and at a few first times after it), shown by /api/heap:
// the difference between two marks is what the part in between keeps. Main loop and setup only.

#include <Arduino.h>
#include <esp_heap_caps.h>

struct HeapMark
{
  const char *name; // a string literal
  uint32_t ms;
  uint32_t freeBytes;
  uint32_t block; // largest free piece
};

#define HEAP_MARKS 24
HeapMark heapMarks[HEAP_MARKS];
uint8_t heapMarkCount = 0;

// Each name is recorded once: a mark placed where the code passes again keeps its first reading.
inline void heapMark(const char *name)
{
  for (uint8_t i = 0; i < heapMarkCount; i++)
    if (heapMarks[i].name == name)
      return;
  if (heapMarkCount >= HEAP_MARKS)
    return;
  HeapMark &m = heapMarks[heapMarkCount++];
  m.name = name;
  m.ms = millis();
  m.freeBytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  m.block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

#endif
