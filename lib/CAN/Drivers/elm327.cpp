#include "elm327.hpp"

#if !defined(ARDUINO_ARCH_ESP8266) && !defined(PIO_UNIT_TESTING)

using namespace CAN;

DriverELM327::DriverELM327(const CAN_Config *CANConfig) : CANDriver(CANConfig)
{
}

bool DriverELM327::ATCommandIsOK()
{
    char result[] = "KO";

    int packetSize = Elm327Serial->available();

    if(packetSize == sizeof(result)) {
        result[0] = Elm327Serial->read();
        result[1] = Elm327Serial->read();

        if(Elm327Serial->read() != '>') {
            return false;
        }
    }

    return strcmp(result, "OK") == 0;
}

bool DriverELM327::setMode(CanDriverMode mode)
{
    switch (mode) {
        case CanDriverMode::Normal:
            listenOnly = false;
            write("AT E0", 5); // disable echo
            if(!ATCommandIsOK()) {
                debugStream->println("CAN-Bus setMode failed! E1");
                return false;
            }
            break;

        case CanDriverMode::Loopback:
            listenOnly = false;
            write("AT E1", 5); // enable echo
            if(!ATCommandIsOK()) {
                debugStream->println("CAN-Bus setMode failed! E2");
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

    while (Elm327Serial->available()) {
        char c = Elm327Serial->read();
        if (c == '>') {
            if(received.length() > 8*2) {
                debugStream->printf("CAN-Bus receive error! Buffer: %s\n", received.c_str());
                received = "";
                continue;
            }

            CanFrame frame;
            uint8_t i = 0;
            frame.len = received.length();
            for(; i < 8; i++) {
                byte byteReceived;

                if(received.length() == 0) {
                    byteReceived = 0;
                } else {
                    byteReceived = strtol(received.substring(0, 2).c_str(), nullptr, 16);
                    received = received.substring(0, 2);
                }

                frame.data[i] = byteReceived;
            }

            onDataRecieved(millis(), frame);

            received = "";
        } else {
            received += c;
        }
    }
}

bool DriverELM327::write(const char *bytes, const size_t size)
{
    if(listenOnly)
        return false;

    size_t len = 0;

    len += Elm327Serial->write(bytes, size);
    len += Elm327Serial->write("\r");

    return len > 0;
}

bool DriverELM327::initInterface()
{
    if(CANConfig->CAN_SPEED_KBPS > 500) {
        debugStream->println("CAN-Bus init failed! E1");
        return false;
    }

    if(CANConfig->CAN_BUS == CAN_ICBus::BT) {
        #ifndef NO_BLUETOOTH
        BluetoothSerial* SerialBT = new BluetoothSerial();
        deleteNeeded = true;

        if(!SerialBT->begin("ESPAltherma", true)) {
            debugStream->println("CAN-Bus init failed! BT - begin failed");
            return false;
        }

        if(CANConfig->CAN_BLUETOOTH.USE_PIN) {
            if(!SerialBT->setPin(CANConfig->CAN_BLUETOOTH.PIN.c_str())) {
                debugStream->println("CAN-Bus init failed! BT - setPin failed");
                return false;
            }
        }

        if (!SerialBT->connect(CANConfig->CAN_BLUETOOTH.DEVICENAME)) {
            debugStream->println("CAN-Bus init failed! BT - connect failed");
            return false;
        }

        Elm327Serial = SerialBT;
        #else
        debugStream->println("CAN-Bus init failed! BT - this firmware is not compiled with bluetooth");
        return false;
        #endif
    } else if(CANConfig->CAN_BUS == CAN_ICBus::UART) {
        Serial1_begin(CANConfig->CAN_UART.PIN_RX, CANConfig->CAN_UART.PIN_TX);

        delay(100);

        if(!Serial1) {
            debugStream->println("CAN-Bus init failed! UART - begin failed");
            return false;
        }

        Elm327Serial = &Serial1;
    } else {
        debugStream->println("CAN-Bus init failed! E2 - not supported IC-Bus found");
        return false;
    }

    write("AT Z", 4);  // just reset ELM327
    if(!ATCommandIsOK()) {
        debugStream->println("CAN-Bus init failed! E3");
        return false;
    }

    uint8_t dividor = 500 / CANConfig->CAN_SPEED_KBPS;
    char baudrateCmd[15];
    sprintf(baudrateCmd, "AT PP 2F SV %02x", dividor);

    write(baudrateCmd, 14); // set given CAN-Bus baudrate
    if(!ATCommandIsOK()) {
        debugStream->println("CAN-Bus init failed! E4");
        return false;
    }

    write("AT PP 2F ON", 11); // Activate/save baud parameter
    if(!ATCommandIsOK()) {
        debugStream->println("CAN-Bus init failed! E5");
        return false;
    }

    write("AT R0", 5); // disable response waiting
    if(!ATCommandIsOK()) {
        debugStream->println("CAN-Bus init failed! E6");
        return false;
    }

    write("AT S0", 5); // disable spaces in return messages
    if(!ATCommandIsOK()) {
        debugStream->println("CAN-Bus init failed! E7");
        return false;
    }

    write("AT SP C", 7);
    if(!ATCommandIsOK()) {
        debugStream->println("CAN-Bus init failed! E8");
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
    if(!ATCommandIsOK()) {
        debugStream->println("CAN-Bus setID failed!");
        return false;
    }

    return true;
}

void DriverELM327::sendCommand(CANCommand* cmd, bool setValue, int value)
{
    if(currentMode == CanDriverMode::ListenOnly)
        return;

    CanFrame* frame = getCanFrameFromCommand(cmd, setValue, value);

    setID(frame->id);

    // convert command to hex string
    char command[16];
    for(uint8_t i = 0; i < CAN_COMMAND_BYTE_LENGTH; i++) {
        sprintf(command + (i*2), "%02x", cmd->command[i]);
    }

    if(!write(command, 16)) {
        debugStream->printf("CAN couldn't send command: %.16s\n", command);
    }

    delete frame;
}

DriverELM327::~DriverELM327()
{
    if(deleteNeeded)
        delete Elm327Serial;
}

#endif