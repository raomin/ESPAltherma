#ifndef ESPALTHERMA_REGISTRYPOLL_H
#define ESPALTHERMA_REGISTRYPOLL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// query() owns framing/checksum validation. Matching bytes left by a failed query are never evidence
// of success. Clear each attempt so even a short later reply cannot inherit an earlier one's payload.
template <typename Query, typename Retry>
bool registryPoll(uint8_t registry, uint8_t *buffer, size_t size, char protocol, Query query, Retry retry)
{
  if (size < 2 || (protocol != 'I' && protocol != 'S'))
    return false;
  for (unsigned int attempt = 0; attempt < 4; attempt++)
  {
    memset(buffer, 0, size);
    bool valid = query();
    uint8_t received = buffer[protocol == 'S' ? 0 : 1];
    if (valid && received == registry && (protocol == 'S' || buffer[0] == 0x40))
      return true;
    if (attempt < 3)
      retry();
  }
  return false;
}

#endif
