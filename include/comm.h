#ifndef COMM_H
#define COMM_H

#include <Arduino.h>
#include <HardwareSerial.h>
HardwareSerial MySerial(1);
#define SERIAL_CONFIG (SERIAL_8E1)
#define SERIAL_FLUSH_TX_ONLY false
#define SER_TIMEOUT 300 //leave 300ms for the machine to answer
#define REPLY_BUFFER_SIZE 64 //size of the buffers given to queryRegistry

// The X10A link is shared by the polling task, the detection survey and the DebugSerial gateway.
// Recursive: the DebugSerial callback runs client.loop() while holding it, which can re-enter it.
SemaphoreHandle_t serialMutex = xSemaphoreCreateRecursiveMutex();
void serialLock() { xSemaphoreTakeRecursive(serialMutex, portMAX_DELAY); }
void serialUnlock() { xSemaphoreGiveRecursive(serialMutex); }

unsigned char getCRC(unsigned char *src, int len)
{
  unsigned char b = 0;
  for (int i = 0; i < len; i++)
  {
    b += src[i];
  }
  return ~b;
}

void logBuffer(unsigned char *buffer, size_t len)
{
  char bufflog[250] = {0};
  for (size_t i = 0; i < len; i++)
  {
    sprintf(bufflog + i * 5, "0x%02x ", buffer[i]);
  }
  mqttSerial.print(bufflog);
}

int get_reply_len(char regID, char protocol='I')
{
  if (protocol == 'I')
  {
    // Backward compatible behavior. Actual length is dynamic and returned
    // on 3rd byte of the response.
    return 12;
  }
  else
  {
    // Protocol S has hard-coded values based on the requested registry
    switch (regID)
    {
      case 0x50:
        return 6;
      case 0x56:
        return 4;
      default:
        return 18;
    }
  }
}

bool queryRegistryUnlocked(char regID, unsigned char *buffer, char protocol);

bool queryRegistry(char regID, unsigned char *buffer, char protocol='I')
{
  serialLock();
  bool ok = queryRegistryUnlocked(regID, buffer, protocol);
  serialUnlock();
  return ok;
}

bool queryRegistryUnlocked(char regID, unsigned char *buffer, char protocol)
{

  //preparing command:
  unsigned char prep[] = {0x03, 0x40, regID, 0x00};
  prep[3] = getCRC(prep, 3);
  int queryLength = 4;

  if (protocol == 'S')
  {
    prep[0] = 0x02;
    prep[1] = regID;
    prep[2] = getCRC(prep, 2);
    prep[3] = 0;
    queryLength = 3;
  }

  mqttSerial.printf("Querying register 0x%02x... ", regID);
  //Sending command to serial
  MySerial.flush(SERIAL_FLUSH_TX_ONLY); //Prevent possible pending info on the read
  MySerial.write((uint8_t*) prep, queryLength);
  ulong start = millis();

  int len = 0;
  int replyLen = get_reply_len(regID, protocol);

  while ((len < replyLen) && (millis() < (start + SER_TIMEOUT)))
  {
    if (!MySerial.available())
    {
      delay(1); //let other tasks run while the bytes arrive (the UART buffers them)
      continue;
    }
    if (len >= REPLY_BUFFER_SIZE)
    {
      mqttSerial.printf("ERR: Reply of register 0x%02x too long (%d bytes)\n", regID, replyLen);
      delay(500);
      return false;
    }
    buffer[len++] = MySerial.read();
    if (protocol == 'I' && len == 3)
    {
      // Override reply length with the actual one (not counting already read bytes, see doc/Daikin I protocol.md)
      replyLen = buffer[2] + 2;
    }
    // Error reply common to both protocols
    if (len == 2 && buffer[0] == 0x15 && buffer[1] == 0xea)
    {
      // HP didn't understand the command
      mqttSerial.printf("Error 0x15 0xEA returned from HP\n");
      delay(500);
      return false;
    }
  }
  if (millis() >= (start + SER_TIMEOUT))
  {
    if (len == 0)
    {
      mqttSerial.printf("Time out! Check connection\n");
    }
    else
    {
      mqttSerial.printf("ERR: Time out on register 0x%02x! got %d/%d bytes\n", regID, len, replyLen);
      logBuffer(buffer, len);
    }
    delay(500);
    return false;
  }
  logBuffer(buffer, len);
  if (getCRC(buffer, len - 1) != buffer[len - 1])
  {
    mqttSerial.printf("ERROR: Wrong CRC on register 0x%02x. Calculated 0x%2x but got 0x%2x\nBuffer: ",regID, getCRC(buffer, len - 1), buffer[len - 1]);
    logBuffer(buffer,len);
    return false;
  }
  else
  {
    Serial.println(".. CRC OK!");
    return true;
  }
}

#endif // COMM_H
