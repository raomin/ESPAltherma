#ifndef ESPALTHERMA_MQTTFRAME_H
#define ESPALTHERMA_MQTTFRAME_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// PubSubClient's beginPublish narrows Remaining Length to 16 bits. Large device-discovery payloads
// need MQTT's full variable-length header. write() still goes through PubSubClient, updating keepalive time.
template <typename Client>
bool mqttBeginStream(Client &client, const char *topic, size_t payloadLength, bool retained)
{
  size_t topicLength = strlen(topic);
  if (!client.connected() || topicLength > 65535 || payloadLength > 268435455UL - topicLength - 2)
    return false;
  uint32_t remaining = (uint32_t)(payloadLength + topicLength + 2);
  uint8_t header[7];
  size_t n = 0;
  header[n++] = retained ? 0x31 : 0x30;
  do {
    uint8_t byte = remaining & 0x7F;
    remaining >>= 7;
    header[n++] = byte | (remaining ? 0x80 : 0);
  } while (remaining);
  header[n++] = (uint8_t)(topicLength >> 8);
  header[n++] = (uint8_t)topicLength;
  return client.write(header, n) == n &&
         client.write(reinterpret_cast<const uint8_t *>(topic), topicLength) == topicLength;
}

#endif
