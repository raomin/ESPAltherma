#include "x10a.hpp"

#ifdef ARDUINO_ARCH_ESP8266
SoftwareSerial SerialX10A;
#define SERIAL_CONFIG (SWSERIAL_8E1)
#else
HardwareSerial SerialX10A(1);
#define SERIAL_CONFIG (SERIAL_8E1)
#endif

static X10A_Config* X10AConfig = nullptr;
size_t registryBufferSize;
RegistryBuffer *registryBuffers; // holds the registries to query and the last returned
ulong lastTimeRunned = 0;

void X10AEnd()
{
  if(SerialX10A) {
    SerialX10A.end();
  }

  if(X10AConfig != nullptr) {
    delete X10AConfig;
  }
}

void initRegistries(RegistryBuffer** buffer, size_t& bufferSize)
{
  // getting the list of registries to query from the selected values
  bufferSize = 0;
  uint8_t* tempRegistryIDs = new uint8_t[X10AConfig->PARAMETERS_LENGTH]();

  size_t i;
  for (i = 0; i < X10AConfig->PARAMETERS_LENGTH; i++) {
    auto &&label = *X10AConfig->PARAMETERS[i];

    if (!contains(tempRegistryIDs, X10AConfig->PARAMETERS_LENGTH, label.registryID)) {
      debugSerial.printf("Adding registry 0x%2x to be queried.\n", label.registryID);
      tempRegistryIDs[bufferSize++] = label.registryID;
    }
  }

  *buffer = new RegistryBuffer[bufferSize];

  for(i = 0; i < bufferSize; i++) {
    (*buffer)[i].RegistryID = tempRegistryIDs[i];
  }

  delete[] tempRegistryIDs;
}

void handleX10A(RegistryBuffer* buffer, const size_t& bufferSize, const bool sendValuesViaMQTT)
{
  // querying all registries and store results
  for (size_t i = 0; i < bufferSize; i++) {
    uint8_t tries = 0;
    while (tries++ < 3 && !queryRegistry(&buffer[i], X10AConfig->X10A_PROTOCOL)) {
      debugSerial.println("Retrying...");
      waitLoop(1000);
    }
  }

  for (size_t i = 0; i < X10AConfig->PARAMETERS_LENGTH; i++) {
    auto &&label = *X10AConfig->PARAMETERS[i];

    for (size_t j = 0; j < bufferSize; j++)
    {
      byte receivedRegistryID;
      uint8_t offset;
      if(X10AConfig->X10A_PROTOCOL == X10AProtocol::S) {
        receivedRegistryID = buffer[j].Buffer[0];
        offset = 1;
      } else {
        receivedRegistryID = buffer[j].Buffer[1];
        offset = 3;
      }

      if(buffer[j].Success && label.registryID == receivedRegistryID) {
        byte *input = buffer[j].Buffer;
        input += label.offset + offset;

        converter.convert(&label, input); // convert buffer result of label offset to correct/usabel value

        if(sendValuesViaMQTT) {
          updateValues(&label); // send them in mqtt
          waitLoop(500);        // wait .5sec between registries
        }

        break;
      }
    }
  }

  if(sendValuesViaMQTT) {
    sendValues(); // send the full json message
  }
}

void X10AInit(X10A_Config* X10AConfigToInit)
{
  X10AEnd();
  X10AConfig = X10AConfigToInit;
  SerialX10A.begin(9600, SERIAL_CONFIG, X10AConfig->PIN_RX, X10AConfig->PIN_TX);
}

void X10A_loop()
{
  // TODO should be catched from main loop run
  ulong loopStart = millis();

  if(loopStart - lastTimeRunned >= X10AConfig->FREQUENCY * 1000) {
    debugSerial.printf("CAN Poll Mode Auto Reading: %lu\n", loopStart);

    handleX10A(registryBuffers, registryBufferSize, true);

    ulong loopEnd = X10AConfig->FREQUENCY - millis() + loopStart;

    debugSerial.printf("Done. Waiting %.2f sec...\n", (float)loopEnd / 1000);
    lastTimeRunned = loopStart;
  }
}