#include "ELM327.hpp"

#define Elm327Serial Serial1

bool DriverELM327::ATCommandIsOK()
{
    char result[] = "KO";

    int packetSize = Elm327Serial.available();

    if (packetSize && packetSize == 2)
    {
        Elm327Serial.read(result, packetSize);
    }

    return strcmp(result, "OK") == 0;
}

bool DriverELM327::setMode(CanDriverMode mode)
{
  switch (mode)
  {
  case CanDriverMode::Normal:
    listenOnly = false;
    write("AT E0", 5); // disable echo
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }
    break;

  case CanDriverMode::Loopback:
    listenOnly = false;
    write("AT E1", 5); // enable echo
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }
    break;

  case CanDriverMode::ListenOnly:
    listenOnly = true;
    break;
  }

  currentMode = mode;

  return true;
}

void DriverELM327::handleLoop()
{
    CANDriver::handleLoop();

    if(!canInited)
        return;

    while (Elm327Serial.available())
    {
        char c = Elm327Serial.read();
        if (c == '>')
        {
            if(received.length() > 8*2)
            {
                debugSerial.printf("CAN-Bus receive error! Buffer: %s\n", received.c_str());
                received = "";
                continue;
            }

            CanFrame frame;
            uint8_t i = 0;
            frame.len = received.length();
            for(; i < 8; i++)
            {
                byte byteReceived;

                if(received.length() == 0)
                    byteReceived = 0;
                else
                {
                    byteReceived = strtol(received.substring(0, 2).c_str(), nullptr, 16);
                    received = received.substring(0, 2);
                }

                frame.data[i] = byteReceived;
            }

            onDataRecieved(millis(), frame);

            received = "";
        }
        else
        {
            received += c;
        }
    }
}

bool DriverELM327::write(const char *bytes, const size_t size)
{
    if(listenOnly)
        return false;

    size_t len = 0;

    len += Elm327Serial.write(bytes, size);
    len += Elm327Serial.write("\r");

    return len > 0;
}

bool DriverELM327::initInterface()
{
    if(config->CAN_SPEED_KBPS > 500)
    {
        debugSerial.println("CAN-Bus init failed! E1");
        return false;
    }

    Elm327Serial.begin(38400, SERIAL_8N1, config->PIN_CAN_RX, config->PIN_CAN_TX);

    write("AT Z", 4);  // just reset ELM327
    //if(read() != "OK")
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }

    uint8_t dividor = 500 / config->CAN_SPEED_KBPS;
    char baudrateCmd[15];
    sprintf(baudrateCmd, "AT PP 2F SV %02x", dividor);

    write(baudrateCmd, 14); // set given CAN-Bus baudrate
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }

    write("AT PP 2F ON", 11); // Activate/save baud parameter
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }

    write("AT R0", 5); // disable response waiting
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }

    write("AT S0", 5); // disable spaces in return messages
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }

    write("AT SP C", 7);
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }

    setMode(CanDriverMode::Normal);

    defaultInit();

    return true;
}

bool DriverELM327::setID(const uint16_t id)
{
    char message[9];
    sprintf (message, "ATSH%X", id);
    write(message, 9);
    if(!ATCommandIsOK())
    {
        // error
        return false;
    }

    return true;
}

void DriverELM327::sendCommand(CommandDef* cmd, bool setValue, int value)
{
    if(currentMode == CanDriverMode::ListenOnly)
        return;

    CanFrame* frame = getCanFrameFromCommand(cmd, setValue, value);

    setID(frame->id);

    // convert command to hex string
    char command[16];
    for(uint8_t i = 0; i < COMMAND_BYTE_LENGTH; i++)
    {
        sprintf(command + (i*2), "%02x", cmd->command[i]);
    }

    if(!write(command, 16))
    {
        debugSerial.printf("CAN couldn't send command: %.16s\n", command);
    }

    delete frame;
}