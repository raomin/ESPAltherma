#include "x10a.hpp"

using namespace X10A;

std::function<void(const ulong ms)> callbackX10A_wait;
std::function<void()> callbackX10A_sendValues;
std::function<void(ParameterDef *labelDef)> callbackX10A_updateValues;

IX10ASerial* X10ASerial = nullptr;

static X10A_Config* X10AConfig = nullptr;
bool disableMQTTLogMessages;
size_t registryBufferSize;
RegistryBuffer *registryBuffers; // holds the registries to query and the last returned
ulong lastTimeRunned = 0;
HandleState handleState = HandleState::Stopped;

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
  debugStream->print(bufflog);
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
  debugStream->printf("Querying register 0x%02x... ", registryBuffer->RegistryID);
  X10ASerial->flush(); // prevent possible pending info on the read
  X10ASerial->write((uint8_t*)prep, queryLength);
  ulong start = millis();

  int len = 0;
  int replyLen = get_reply_len(registryBuffer->RegistryID, protocol);
  while ((len < replyLen) && (millis() < (start + SER_TIMEOUT))) {
    if (X10ASerial->available()) {
      registryBuffer->Buffer[len++] = X10ASerial->read();
      if (protocol == X10AProtocol::I && len == 3) {
        // Override reply length with the actual one (not counting already read bytes, see doc/Daikin I protocol.md)
        replyLen = registryBuffer->Buffer[2] + 2;
      }
      if (len == 2 && registryBuffer->Buffer[0] == 0x15 && registryBuffer->Buffer[1] == 0xEA) {
        // HP didn't understand the command
        debugStream->println("Error 0x15 0xEA returned from HP");
        delay(500);
        return false;
      }
    }
  }
  if (millis() >= (start + SER_TIMEOUT)) {
    if (len == 0) {
      debugStream->printf("Time out! Check connection\n");
    } else {
      debugStream->printf("ERR: Time out on register 0x%02x! got %d/%d bytes\n", registryBuffer->RegistryID, len, replyLen);
      logBuffer(registryBuffer->Buffer, len);
    }
    delay(500);
    return false;
  }

  registryBuffer->CRC = getCRC(registryBuffer->Buffer, len - 1);
  logBuffer(registryBuffer->Buffer, len);
  if (registryBuffer->CRC != registryBuffer->Buffer[len - 1]) {
    debugStream->println("Wrong CRC!");
    debugStream->printf("ERROR: Wrong CRC on register 0x%02x!", registryBuffer->RegistryID);
    debugStream->printf("Calculated 0x%2x but got 0x%2x\nnBuffer: ", registryBuffer->CRC, registryBuffer->Buffer[len - 1]);
    logBuffer(registryBuffer->Buffer, len);
    return false;
  } else {
    debugStream->println(".. CRC OK!");
    registryBuffer->Success = true;
    return true;
  }
}

void x10a_end()
{
  if(X10ASerial) {
    X10ASerial->end();
  }

  if(X10ASerial != nullptr) {
    delete X10ASerial;
  }

  if(X10AConfig != nullptr) {
    delete X10AConfig;
  }
}

void x10a_initRegistries(RegistryBuffer** buffer, size_t& bufferSize)
{
  // getting the list of registries to query from the selected values
  bufferSize = 0;
  uint8_t* tempRegistryIDs = new uint8_t[X10AConfig->PARAMETERS_LENGTH]();

  size_t i;
  for (i = 0; i < X10AConfig->PARAMETERS_LENGTH; i++) {
    auto &&label = *X10AConfig->PARAMETERS[i];

    if (!contains(tempRegistryIDs, X10AConfig->PARAMETERS_LENGTH, label.registryID)) {
      debugStream->printf("Adding registry 0x%2x to be queried.\n", label.registryID);
      tempRegistryIDs[bufferSize++] = label.registryID;
    }
  }

  *buffer = new RegistryBuffer[bufferSize];

  for(i = 0; i < bufferSize; i++) {
    (*buffer)[i].RegistryID = tempRegistryIDs[i];
  }

  delete[] tempRegistryIDs;

  lastTimeRunned = -(X10AConfig->FREQUENCY);
}

void x10a_handle(RegistryBuffer* buffer, const size_t& bufferSize, const bool sendValuesViaMQTT)
{
  // querying all registries and store results
  for (size_t i = 0; i < bufferSize; i++) {
    uint8_t tries = 0;
    while (tries++ < 3 && !queryRegistry(&buffer[i], X10AConfig->X10A_PROTOCOL)) {
      debugStream->println("Retrying...");
      callbackX10A_wait(1000);
    }
  }

  x10a_convert_values(buffer, bufferSize, sendValuesViaMQTT);

  if(sendValuesViaMQTT && !disableMQTTLogMessages) {
    callbackX10A_sendValues(); // send the full json message
  }
}

void x10a_convert_values(RegistryBuffer* buffer, const size_t& bufferSize, const bool sendValuesViaMQTT) {
  for (size_t i = 0; i < X10AConfig->PARAMETERS_LENGTH; i++) {
    auto&& label = *X10AConfig->PARAMETERS[i];

    for (size_t j = 0; j < bufferSize; j++) {
      byte receivedRegistryID;
      uint8_t offset;
      if (X10AConfig->X10A_PROTOCOL == X10AProtocol::S) {
        receivedRegistryID = buffer[j].Buffer[0];
        offset = 1;
      } else {
        receivedRegistryID = buffer[j].Buffer[1];
        offset = 3;
      }

      if (buffer[j].Success && label.registryID == receivedRegistryID) {
        byte* input = buffer[j].Buffer;
        input += label.offset + offset;

        converter.convert(&label, input);  // convert buffer result of label offset to correct/usabel value

        if (sendValuesViaMQTT) {
          callbackX10A_updateValues(&label);  // send them in mqtt
          callbackX10A_wait(500);         // wait .5sec between registries
        }

        break;
      }
    }
  }
}

void x10a_set_serial(IX10ASerial* serial)
{
  X10ASerial = serial;
}

void x10a_init(X10A_Config* X10AConfigToInit, const bool disableMQTTLogMessagesToInit)
{
  x10a_end();
  X10AConfig = X10AConfigToInit;
  disableMQTTLogMessages = disableMQTTLogMessagesToInit;
  X10ASerial->begin(9600, X10AConfig->PIN_RX, X10AConfig->PIN_TX);
}

void x10a_loop()
{
  if(handleState == HandleState::Running)
    return;

  // TODO should be catched from main loop run
  ulong loopStart = millis();

  if(loopStart - lastTimeRunned >= X10AConfig->FREQUENCY) {
    handleState = HandleState::Running;
    debugStream->printf("X10A started reading: %lu\n", loopStart);

    x10a_handle(registryBuffers, registryBufferSize, true);

    ulong loopEnd = X10AConfig->FREQUENCY - millis() + loopStart;

    debugStream->printf("X10A Done. Waiting %.2f sec...\n", (float)loopEnd / 1000);
    lastTimeRunned = loopStart;
    handleState = HandleState::Stopped;
  }
}

void x10a_fill_config(JsonObject &jsonObject, X10A_Config *config) {
  JsonArray parameters = jsonObject["PARAMETERS"].as<JsonArray>();
  config->PARAMETERS_LENGTH = parameters.size();
  config->PARAMETERS = new ParameterDef *[config->PARAMETERS_LENGTH];

  for (size_t i = 0; i < config->PARAMETERS_LENGTH; i++) {
    JsonArray parameter = parameters[i];
    config->PARAMETERS[i] = new ParameterDef(
        parameter[0].as<const int>(),
        parameter[1].as<const int>(),
        parameter[2].as<const int>(),
        parameter[3].as<const int>(),
        parameter[4].as<const int>(),
        parameter[5]);
  }
}

bool x10a_got_inited()
{
  return X10ASerial;
}