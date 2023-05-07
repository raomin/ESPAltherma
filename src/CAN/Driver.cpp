#include "Driver.hpp"

// tranlated from fhemHPSU and pyHPSU github project.
// https://github.com/ahermann86/fhemHPSU
// https://github.com/zanac/pyHPSU

String** resultBuffer;

CanFrame* CANDriver::getCanFrameFromCommand(CANCommand* cmd, bool setValue, int value)
{
  CanFrame* frame = new CanFrame();

  if(setValue)
  {
      frame->id = 680;
  }
  else
  {
      frame->id = cmd->id;
  }

  frame->len = sizeof(cmd->command);
  memcpy(frame->data, cmd->command, frame->len);

  if(cmd->writable && setValue)
  {
      // set first byte in command array to have HEX Value "0" on second position
      // character No 2: 0=write 1=read 2=answer
      frame->data[0] = (frame->data[0] & 0xF0) | 0x00;

      byte valByte1 = 0;
      byte valByte2 = 0;

      if(value < 0 && strcmp(cmd->type, "float") != 0)
      {
          // error
          // set negative values if type not float not possible !!!
          delete frame;
          return nullptr;
      }

      const double calculatedValue = value * cmd->divisor;

      if(strcmp(cmd->type, "int") == 0)
      {
          valByte1 = calculatedValue;
      }
      else if(strcmp(cmd->type, "value") == 0)
      {
          valByte1 = calculatedValue;
      }
      else if(strcmp(cmd->type, "longint") == 0)
      {
          valByte1 = (int)calculatedValue >> 8;
          valByte2 = (int)calculatedValue & 0xFF;
      }
      else if(strcmp(cmd->type, "float") == 0)
      {
          const int intCalcValue = (int)calculatedValue & 0xFFFF;
          valByte1 = intCalcValue >> 8;
          valByte2 = intCalcValue & 0xFF;
      }

      if (frame->data[2] == 0xFA)  // 2=pos address
      {
          // Byte 3 == FA
          // 30 0A FA 01 D6 00 D9   <- $CANMsg
          //                |  ^pos: 6
          //                ^pos: 5
          frame->data[5] = valByte1;
          frame->data[6] = valByte2;
      }
      else
      {
          // Byte 3 != FA
          // 30 0A 0E 01 E8 00 00   <- $CANMsg
          //          |  ^pos: 4
          //          ^pos: 3
          //    t_dhw - 48,8Â°
          frame->data[3] = valByte1;
          frame->data[4] = valByte2;
      }
  }
  else
  {
    for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++)
    {
      if(cmdSendInfos[i]->cmd == cmd)
      {
        cmdSendInfos[i]->pending = true;
        cmdSendInfos[i]->timeMessageSend = millis();
        break;
      }
    }
  }

  debugSerial.printf("CAN: Transmiting ID(%i) ", frame->id);
  for(uint8_t i = 0; i < frame->len; i++)
  {
    debugSerial.printf("%02x ", frame->data[i]);
  }
  debugSerial.println();

  return frame;
}

void CANDriver::sniffCAN(const uint32_t timestamp_us, CanFrame const frame)
{
  char resultText[64] = "";
  sprintf(resultText, "CAN [ %i ] ID", timestamp_us);

  if(frame.isRTR) strcat(resultText, "(RTR)");
  if(frame.isEXT) strcat(resultText, "(EXT)");

  sprintf(resultText + strlen(resultText), " %02X DATA[%i] ", frame.id, frame.len);

  for(uint8_t i = 0; i < frame.len; i++)
    sprintf(resultText + strlen(resultText), "%02X ", frame.data[i]);

  debugSerial.println(resultText);
}

CANDriver::CANDriver(const CAN_Config* CANConfigArg) : CANConfig(CANConfigArg)
{
}

void CANDriver::handleLoop()
{
  if(!canInited)
      return;

  checkPendingMessages();

  if(CANConfig->CAN_AUTOPOLL_MODE == CAN_PollMode::Auto)
  {
    ulong currentTime = millis();

    if(currentTime - lastTimeRunned >= CANConfig->CAN_AUTOPOLL_TIME * 1000)
    {
      debugSerial.printf("CAN Poll Mode Auto Reading: %lu\n", currentTime);

      for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++)
      {
        if(cmdSendInfos[i]->pending == false)
        {
          sendCommand(CANConfig->COMMANDS[i], false);
        }
      }

      lastTimeRunned = currentTime;
    }
  }
}

String CANDriver::readAllCommands()
{
  CANDataToString = true;

  resultBuffer = new String*[CANConfig->COMMANDS_LENGTH];

  for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++) {
    currentDataToStringIndex = i;
    resultBuffer[i] = nullptr;
    sendCommand(CANConfig->COMMANDS[i], false);

    while(cmdSendInfos[i]->pending) {
      delay(5);
      checkPendingMessages();
    }
  }

  DynamicJsonDocument resultDoc(CANConfig->COMMANDS_LENGTH*JSON_ARRAY_SIZE(2));
  JsonArray obj = resultDoc.to<JsonArray>();

  for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++) {
    if(resultBuffer[i] != nullptr) {
      obj.add(resultBuffer[i]->c_str());
      delete resultBuffer[i];
    } else {
      // TODO highlight this result orange or red on webui
      obj.add("[Timeout]");
    }
  }
  delete[] resultBuffer;

  CANDataToString = false;

  String result;
  serializeJson(resultDoc, result);
  return result;
}

void CANDriver::onDataRecieved(uint32_t const timestamp_us, CanFrame const frame)
{
  if(!canInited)
      return;

  if(sniffingEnabled || currentMode == CanDriverMode::Loopback)
  {
    sniffCAN(timestamp_us, frame);

    if(currentMode == CanDriverMode::Loopback)
      return;
  }

  if(frame.len < 2)
    return;

  // skip message if it is not a answer
  if((frame.data[0] & 0x0F) != 2)
    return;

  bool extended = frame.data[2] == 0xFA;

  CANCommand* recievedCommand = getCommandFromData(frame.data);

  // if we got a message that we shouldnt handle, skip it
  if(recievedCommand == nullptr)
    return;

  if(CANConfig->CAN_AUTOPOLL_MODE == CAN_PollMode::Auto || CANDataToString)
  {
    for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++)
    {
      if(cmdSendInfos[i]->cmd == recievedCommand)
      {
        // if we didnt fetch the infos, ignore it
        if(!cmdSendInfos[i]->pending)
        {
          return;
        }

        cmdSendInfos[i]->pending = false;
        break;
      }
    }
  }

  byte valByte1 = 0;
  byte valByte2 = 0;

  if (extended)  // -> Byte3 eq FA
  {
    //20 0A FA 01 D6 00 D9   <- $CANMsg
    //               |  ^pos: 6
    //               ^pos: 5
    //   t_hs - 21,7

    valByte1 = frame.data[5];
    valByte2 = frame.data[6];
  }
  else
  {
    //20 0A 0E 01 E8 00 00   <- $CANMsg
    //         |  ^pos: 4
    //         ^pos: 3
    //   t_dhw - 48,8Â°
    valByte1 = frame.data[3];
    valByte2 = frame.data[4];
  }

  int value;

  if(strcmp(recievedCommand->type, "int") == 0)
  {
    value = HPSU_toSigned(valByte1, recievedCommand->unit);
  }
  else if(strcmp(recievedCommand->type, "value") == 0)
  {
    value = valByte1;
    //example: mode_01 val 4       -> 31 00 FA 01 12 04 00
  }
  else if(strcmp(recievedCommand->type, "longint") == 0)
  {
    value = HPSU_toSigned(valByte2 + valByte1 * 0x0100, recievedCommand->unit);
    //example: one_hot_water val 1 -> 31 00 FA 01 44 00 01
    //                                                   ^
  }
  else if(strcmp(recievedCommand->type, "float") == 0)
  {
    value = HPSU_toSigned(valByte2 + valByte1 * 0x0100, recievedCommand->unit);
  }
  else
  {
    return;
  }

  value /= recievedCommand->divisor;

  String valueCodeKey = String(value);

  if(recievedCommand->valueCodeSize > 0)
  {
    for (byte counter = 0; counter < recievedCommand->valueCodeSize; counter++)
    {
      if(recievedCommand->valueCode[counter]->value.toInt() == value)
      {
        valueCodeKey = recievedCommand->valueCode[counter]->key;
        break;
      }
    }
  }

  if(strlen(recievedCommand->unit) > 0)
  {
      if(strcmp(recievedCommand->unit, "deg") == 0)
      {
        valueCodeKey += " °C";
      }
      else if(strcmp(recievedCommand->unit, "percent") == 0)
      {
        valueCodeKey += " %";
      }
      else if(strcmp(recievedCommand->unit, "bar") == 0)
      {
        valueCodeKey += " bar";
      }
      else if(strcmp(recievedCommand->unit, "kwh") == 0)
      {
        valueCodeKey += " kWh";
      }
      else if(strcmp(recievedCommand->unit, "kw") == 0)
      {
        valueCodeKey += " kW";
      }
      else if(strcmp(recievedCommand->unit, "w") == 0)
      {
        valueCodeKey += " W";
      }
      else if(strcmp(recievedCommand->unit, "sec") == 0)
      {
        valueCodeKey += " sec";
      }
      else if(strcmp(recievedCommand->unit, "min") == 0)
      {
        valueCodeKey += " min";
      }
      else if(strcmp(recievedCommand->unit, "hour") == 0)
      {
        valueCodeKey += " h";
      }
      else if(strcmp(recievedCommand->unit, "lh") == 0)
      {
        valueCodeKey += " lh";
      }
  }

  if(CANDataToString) {
    resultBuffer[currentDataToStringIndex] = new String(valueCodeKey);
    return;
  }

  if(config->MQTT_USE_ONETOPIC)
  {
    client.publish((config->MQTT_TOPIC_NAME + config->MQTT_ONETOPIC_NAME + CANConfig->CAN_MQTT_TOPIC_NAME + recievedCommand->label).c_str(), valueCodeKey.c_str());
  }
  else
  {
    client.publish((config->MQTT_TOPIC_NAME + CANConfig->CAN_MQTT_TOPIC_NAME + recievedCommand->label).c_str(), valueCodeKey.c_str());
  }

  debugSerial.printf("CAN Data recieved %s: %s\n", recievedCommand->label, valueCodeKey.c_str());
}


CANCommand* CANDriver::getCommandFromData(const uint8_t *data)
{
  bool extended = data[2] == 0xFA;
  CANCommand* recievedCommand = nullptr;

  for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++)
  {
    //Byte 3 == FA
    //31 00 FA 0B D1 00 00   <- $CANMsg
    //      |------| -> len: 3 byte
    //      ^pos: 2
    if(extended &&
       CANConfig->COMMANDS[i]->command[2] == data[2] &&
       CANConfig->COMMANDS[i]->command[3] == data[3] &&
       CANConfig->COMMANDS[i]->command[4] == data[4])
    {
      recievedCommand = CANConfig->COMMANDS[i];
      break;
    }
    //Byte 3 != FA
    //31 00 05 00 00 00 00   <- $CANMsg
    //      || -> len: 1 byte
    //      ^pos: 2
    else if(!extended && CANConfig->COMMANDS[i]->command[2] == data[2])
    {
      recievedCommand = CANConfig->COMMANDS[i];
      break;
    }
  }

  return recievedCommand;
}

void CANDriver::listenOnly(bool value)
{
  if(value)
    setMode(CanDriverMode::ListenOnly);
  else
    setMode(CanDriverMode::Normal);
}

int CANDriver::HPSU_toSigned(uint16_t value, char* unit)
{
  if(strcmp(unit, "deg") == 0 || strcmp(unit, "value_code_signed") == 0) {
    int newValue = value & 0xFFFF;
    return (newValue ^ 0x8000) - 0x8000;
  } else {
    return value;
  }
}

void CANDriver::enableSniffing(bool value)
{
  sniffingEnabled = value;
}

void CANDriver::handleMQTTSetRequest(const String &label, const char *payload, const uint32_t length)
{
  if(!canInited)
      return;

  const int payloadAsInt = atoi(payload);

  for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++) {
    if(CANConfig->COMMANDS[i]->writable && strcmp(CANConfig->COMMANDS[i]->name, label.c_str()) == 0) {
      debugSerial.printf("CAN: Got MQTT SET request for %s, %08x\n", label.c_str(), payloadAsInt);
      sendCommand(CANConfig->COMMANDS[i], true, payloadAsInt);
      return;
    }
  }

  debugSerial.printf("CAN: Got invalid MQTT SET request for %s\n", label.c_str());
}

void CANDriver::defaultInit()
{
  cmdSendInfos = new CMDSendInfo*[CANConfig->COMMANDS_LENGTH];
  for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++) {
    cmdSendInfos[i] = new CMDSendInfo();
    cmdSendInfos[i]->cmd = CANConfig->COMMANDS[i];
  }

  callbackCAN = [this](const String label, const char *payload, const uint32_t length) { handleMQTTSetRequest(label, payload, length); };

  canInited = true;

  listenOnly(CANConfig->CAN_READONLY_ENABLED);

  debugSerial.println("CAN-Bus inited");
}

void CANDriver::checkPendingMessages()
{
  uint64_t currentMillis = millis();

  for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++) {
    if(cmdSendInfos[i]->pending == true && currentMillis - cmdSendInfos[i]->timeMessageSend >= CAN_MESSAGE_TIMEOUT * 1000) {
      cmdSendInfos[i]->pending = false;
      debugSerial.printf("CAN Timeout for message: %s\n", cmdSendInfos[i]->cmd->label);
    }
  }
}

CANDriver::~CANDriver() {
  if(canInited) {
    for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++) {
      delete cmdSendInfos[i];
    }
    delete[] cmdSendInfos;
  }
}