#ifndef ESPALTHERMA_REGISTRYFRAME_H
#define ESPALTHERMA_REGISTRYFRAME_H

#include <stdint.h>
#include <stddef.h>

inline size_t registryFrameSizeS(uint8_t reg) { return reg == 0x50 ? 6 : reg == 0x56 ? 4 : 18; }

// Length is the received frame length, not the capacity of its receive buffer. The checksum is never payload.
inline bool registryFramePayload(const uint8_t *frame, size_t length, char protocol, size_t &offset, size_t &size)
{
  offset = size = 0;
  if (!frame || length < 2)
    return false;
  if (protocol == 'I')
  {
    if (length < 4 || frame[0] != 0x40 || frame[2] < 2 || (size_t)frame[2] + 2 != length)
      return false;
    offset = 3;
  }
  else if (protocol == 'S')
  {
    if (length != registryFrameSizeS(frame[0]))
      return false;
    offset = 1;
  }
  else
    return false;
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++)
    sum += frame[i];
  if (sum != 0xFF)
    return false;
  size = length - offset - 1;
  return true;
}

#endif
