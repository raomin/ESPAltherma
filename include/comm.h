#include <Arduino.h>
#include <HardwareSerial.h>

HardwareSerial MySerial(1);
#define SER_TIMEOUT 400 //leave 400ms for the machine to answer

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
  //test:
  // Serial.printf("testing with sample data\n");
  // // char in[]={0x40,0x60,0x13,0x80,0x00,0x18,0x00,0x00,0x00,0x00,0xC2,0x01,0xC2,0x01,0xE0,0x02,0x23,0x91,0x82,0x00,0x16};
  //   char r61[]={0x40,0x61,0x12,0x80,0x00,0xC1,0x01,0xC1,0x01,0x63,0x01,0x59,0x01,0xD4,0x01,0x02,0x01,0x00,0x00,0xB2};
  //   char r60[]={0x40,0x60,0x13,0x80,0x00,0x40,0x00,0x00,0x00,0x00,0xc2,0x01,0x80,0x01,0xe0,0x02,0x23,0x91,0x82,0x00,0x30};
  //   int lenb=0;
  //   if (regID==0x61){
  //     lenb=sizeof(r61);
  //     memcpy(buffer,r61,sizeof(r61));
  //   }
  //   if (regID==0x60){
  //     lenb=sizeof(r60);
  //     memcpy(buffer,r60,sizeof(r60));
  //   }
  //    if (getCRC(buffer, lenb-1) != buffer[lenb-1]){
  //     Serial.printf("Wrong CRC!: 0x%x at pos %d \n",buffer[lenb-1], lenb-1);
  //     return;
  //   }
  // Serial.printf("done\n");
  //   return;

    //pending unexpected messages?
    char readbuf[200];
    int pos=0;
    while (MySerial.available())
    {
      readbuf[pos++] = MySerial.read();
    }
    if (pos>0){
      char bufflog[250]="Unexpected message: ";
      for (size_t i = strlen(bufflog); i < pos; i++)
      {
          sprintf(bufflog+i*5,"0x%02x ",readbuf[i]);
      }
      mqttSerial.print(bufflog);
    }


  //preparing command:
  char prep[] = {0x03, 0x40, regID, 0x00};
  prep[3] = getCRC(prep, 3);
  memcpy(buffer, prep, 4);

  //Sending command to serial
  MySerial.write(prep);
  // MySerial.write(prep);
  ulong start = millis();

  int len = 0;
  buffer[2] = 10;
  Serial.printf("Querying register 0x%02x... ", regID);
  while ((len < buffer[2] + 2) && (millis() < (start + SER_TIMEOUT)))
  {
    if (MySerial.available())
    {
      buffer[len++] = MySerial.read();
      // Serial.printf("0x%02x ", buffer[len - 1]);
    }
  }
  if (millis() >= (start + SER_TIMEOUT))
  {
    if (len == 0)
    {
      Serial.printf("Time out! Check connection\n");
      /* code */
    }
    else
    {
      mqttSerial.printf("ERR: Time out on register 0x%02x! got %d/%d bytes\n", regID, len, buffer[2]);
      char bufflog[250]={0};
      for (size_t i = 0; i < len; i++)
      {
          sprintf(bufflog+i*5,"0x%02x ",buffer[i]);
      }
      mqttSerial.print(bufflog);
    }

    delay(1000);
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