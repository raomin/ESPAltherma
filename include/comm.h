#include <Arduino.h>
#include <HardwareSerial.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
SoftwareSerial MySerial;
#define SERIAL_CONFIG (SWSERIAL_8E1)
#define SERIAL_FLUSH_TX_ONLY // empty, as SoftwareSerial.flush() takes no parameter
#else
HardwareSerial MySerial(1);
#define SERIAL_CONFIG (SERIAL_8E1)
#define SERIAL_FLUSH_TX_ONLY false
#endif
#define SER_TIMEOUT 300 //leave 300ms for the machine to answer

char getCRC(unsigned char *src, int len)
{
  char b = 0;
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

bool queryRegistry(char regID, unsigned char *buffer, char protocol='I')
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
    if (MySerial.available())
    {
      buffer[len++] = MySerial.read();
      if (protocol == 'I' && len == 3)
      {
        // Override reply length with the actual one
        replyLen = buffer[2];
      }
      if (len == 2 && buffer[0] == 0x15 && buffer[1] == 0xea)
      {
        // HP didn't understand the command
        mqttSerial.printf("Error 0x15 0xEA returned from HP\n");
        Serial.println("Error 0x15 0xEA returned from HP\n");
        delay(500);
        return false;
      }
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
  // logBuffer(buffer, len);
  if (getCRC(buffer, len - 1) != buffer[len - 1])
  {
    Serial.println("Wrong CRC!");
    mqttSerial.printf("ERROR: Wrong CRC on register 0x%02x!", regID);
    Serial.printf("Calculated 0x%2x but got 0x%2x\n", getCRC(buffer, len - 1), buffer[len - 1]);
    return false;
  }
  else
  {
    Serial.println(".. CRC OK!");
    return true;
  }
}
