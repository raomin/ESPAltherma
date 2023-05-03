#include "X10A.hpp"

#ifdef ARDUINO_ARCH_ESP8266
SoftwareSerial SerialX10A;
#define SERIAL_CONFIG (SWSERIAL_8E1)
#else
HardwareSerial SerialX10A(1);
#define SERIAL_CONFIG (SERIAL_8E1)
#endif

size_t registryBufferSize;
RegistryBuffer *registryBuffers; // holds the registries to query and the last returned data

void X10AEnd()
{
  if(SerialX10A)
  {
    SerialX10A.end();
  }
}

void initRegistries(RegistryBuffer** buffer, size_t& bufferSize, ParameterDef** parameters, const size_t parametersLength)
{
  // getting the list of registries to query from the selected values
  bufferSize = 0;
  uint8_t* tempRegistryIDs = new uint8_t[parametersLength]();

  size_t i;
  for (i = 0; i < parametersLength; i++) {
    auto &&label = *parameters[i];

    if (!contains(tempRegistryIDs, parametersLength, label.registryID)) {
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

void handleX10A(RegistryBuffer* buffer, const size_t& bufferSize, ParameterDef** parameters, const size_t parametersLength, const bool sendValuesViaMQTT, X10AProtocol protocol)
{
  // querying all registries and store results
  for (size_t i = 0; i < bufferSize; i++) {
    uint8_t tries = 0;
    while (tries++ < 3 && !queryRegistry(&buffer[i], protocol)) {
      debugSerial.println("Retrying...");
      waitLoop(1000);
    }
  }

  for (size_t i = 0; i < parametersLength; i++) {
    auto &&label = *parameters[i];

    for (size_t j = 0; j < bufferSize; j++)
    {
      byte receivedRegistryID;
      uint8_t offset;
      if(protocol == X10AProtocol::S) {
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

void X10AInit(int8_t rxPin, int8_t txPin)
{
  X10AEnd();
  SerialX10A.begin(9600, SERIAL_CONFIG, rxPin, txPin);
}