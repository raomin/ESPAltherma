#include "comm.hpp"

#ifdef ARDUINO_ARCH_ESP8266
#define SERIAL_FLUSH_TX_ONLY // empty, as SoftwareSerial.flush() takes no parameter
#else
#define SERIAL_FLUSH_TX_ONLY false
#endif

byte getCRC(byte *src, int len)
{
  byte b = 0;
  for (int i = 0; i < len; i++) {
    b += src[i];
  }
  return ~b;
}

void logBuffer(byte *buffer, size_t len)
{
  char bufflog[MAX_BUFFER_SIZE * 5] = {0};
  for (size_t i = 0; i < len; i++) {
    sprintf(bufflog + i * 5, "0x%02x ", buffer[i]);
  }
  debugSerial.print(bufflog);
}

int get_reply_len(char regID, X10AProtocol protocol)
{
  if (protocol == X10AProtocol::I) {
    // Backward compatible behavior. Actual length is dynamic and returned
    // on 3rd byte of the response.
    return 12;
  } else {
    // Protocol S has hard-coded values based on the requested registry
    switch (regID) {
      case 0x50:
        return 6;
      case 0x56:
        return 4;
      default:
        return 18;
    }
  }
}

bool queryRegistry(RegistryBuffer *registryBuffer, X10AProtocol protocol)
{
  // clear buffer first
  memset(registryBuffer->Buffer, 0, sizeof(registryBuffer->Buffer));
  registryBuffer->CRC = 0;
  registryBuffer->Success = false;

  // preparing command:
  byte prep[4];
  uint8_t queryLength;
  if (protocol == X10AProtocol::S) {
    prep[0] = 0x02;
    prep[1] = registryBuffer->RegistryID;
    prep[2] = getCRC(prep, 2);
    prep[3] = 0x00;
    queryLength = 3;
  } else {
    prep[0] = 0x03;
    prep[1] = 0x40;
    prep[2] = registryBuffer->RegistryID;
    prep[3] = 0x00;
    prep[3] = getCRC(prep, 3);
    queryLength = 4;
  }

  // sending command to serial
  debugSerial.printf("Querying register 0x%02x... ", registryBuffer->RegistryID);
  SerialX10A.flush(SERIAL_FLUSH_TX_ONLY); // prevent possible pending info on the read
  SerialX10A.write((uint8_t*)prep, queryLength);
  ulong start = millis();

  int len = 0;
  int replyLen = get_reply_len(registryBuffer->RegistryID, protocol);
  while ((len < replyLen) && (millis() < (start + SER_TIMEOUT))) {
    if (SerialX10A.available()) {
      registryBuffer->Buffer[len++] = SerialX10A.read();
      if (protocol == X10AProtocol::I && len == 3) {
        // Override reply length with the actual one (not counting already read bytes, see doc/Daikin I protocol.md)
        replyLen = registryBuffer->Buffer[2] + 2;
      }
      if (len == 2 && registryBuffer->Buffer[0] == 0x15 && registryBuffer->Buffer[1] == 0xEA) {
        // HP didn't understand the command
        debugSerial.println("Error 0x15 0xEA returned from HP");
        delay(500);
        return false;
      }
    }
  }
  if (millis() >= (start + SER_TIMEOUT)) {
    if (len == 0) {
      debugSerial.printf("Time out! Check connection\n");
    } else {
      debugSerial.printf("ERR: Time out on register 0x%02x! got %d/%d bytes\n", registryBuffer->RegistryID, len, replyLen);
      logBuffer(registryBuffer->Buffer, len);
    }
    delay(500);
    return false;
  }

  registryBuffer->CRC = getCRC(registryBuffer->Buffer, len - 1);
  logBuffer(registryBuffer->Buffer, len);
  if (registryBuffer->CRC != registryBuffer->Buffer[len - 1]) {
    debugSerial.println("Wrong CRC!");
    debugSerial.printf("ERROR: Wrong CRC on register 0x%02x!", registryBuffer->RegistryID);
    debugSerial.printf("Calculated 0x%2x but got 0x%2x\nnBuffer: ", registryBuffer->CRC, registryBuffer->Buffer[len - 1]);
    logBuffer(registryBuffer->Buffer, len);
    return false;
  } else {
    debugSerial.println(".. CRC OK!");
    registryBuffer->Success = true;
    return true;
  }
}