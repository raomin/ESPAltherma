#include <Arduino.h>
#include <HardwareSerial.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
SoftwareSerial MySerial;
#define SERIAL_CONFIG (SWSERIAL_8E1)
#else
HardwareSerial MySerial(1);
#define SERIAL_CONFIG (SERIAL_8E1)
#endif
#define SER_TIMEOUT 300 //leave 300ms for the machine to answer

char getCRC(char *src, int len)
{
  char b = 0;
  for (int i = 0; i < len; i++)
  {
    b += src[i];
  }
  return ~b;
}

bool queryRegistry(char regID, char *buffer)
{

  //preparing command:
  char prep[] = {0x03, 0x40, regID, 0x00};
  prep[3] = getCRC(prep, 3);

  //Sending command to serial
  MySerial.flush(); //Prevent possible pending info on the read
  MySerial.write((uint8_t*) prep, 4);
  ulong start = millis();

  int len = 0;
  buffer[2] = 10;
  mqttSerial.printf("Querying register 0x%02x... ", regID);
  while ((len < buffer[2] + 2) && (millis() < (start + SER_TIMEOUT)))
  {
    if (MySerial.available())
    {
      buffer[len++] = MySerial.read();
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
      mqttSerial.printf("ERR: Time out on register 0x%02x! got %d/%d bytes\n", regID, len, buffer[2]);
      char bufflog[250] = {0};
      for (int i = 0; i < len; i++)
      {
        sprintf(bufflog + i * 5, "0x%02x ", buffer[i]);
      }
      mqttSerial.print(bufflog);
    }
    delay(500);
    return false;
  }
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