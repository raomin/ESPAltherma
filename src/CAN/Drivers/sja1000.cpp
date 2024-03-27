#include "sja1000.hpp"

#ifndef ARDUINO_ARCH_ESP8266

DriverSJA1000::DriverSJA1000(const CAN_Config *CANConfig) : CANDriver(CANConfig)
{
}

bool DriverSJA1000::getRate(const uint16_t speed, twai_timing_config_t &t_config)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    bool found = true;

    switch (speed) {
#if (SOC_TWAI_BRP_MAX > 256)
        case 10:
            t_config = TWAI_TIMING_CONFIG_10KBITS();
            break;
#endif
        case 20:
            if(chip.revision > 1)
                t_config = TWAI_TIMING_CONFIG_20KBITS();
            else
            {
                debugSerial.println("Tried to init CAN-Bus with 20kbps on Revision 0 or Revision 1 Chip!");
                found = false;
            }
            break;

        case 50:
        t_config = TWAI_TIMING_CONFIG_50KBITS();
        break;

        case 100:
        t_config = TWAI_TIMING_CONFIG_100KBITS();
        break;

        case 125:
        t_config = TWAI_TIMING_CONFIG_125KBITS();
        break;

        case 250:
        t_config = TWAI_TIMING_CONFIG_250KBITS();
        break;

        case 500:
        t_config = TWAI_TIMING_CONFIG_500KBITS();
        break;

        case 800:
        t_config = TWAI_TIMING_CONFIG_800KBITS();
        break;

        case 1000:
        t_config = TWAI_TIMING_CONFIG_1MBITS();
        break;

        default:
        found = false;
        break;
    }

    return found;
}

bool DriverSJA1000::initInterface()
{
    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    bool ratePossible = getRate(CANConfig->CAN_SPEED_KBPS, t_config);

    if(!ratePossible) {
        debugSerial.println("CAN-Bus init failed! E1");
        return false;
    }

    defaultInit();

    return true;
}

void DriverSJA1000::sendCommand(CANCommand* cmd, bool setValue, int value)
{
    if(currentMode == CanDriverMode::ListenOnly)
        return;

    CanFrame* frame = getCanFrameFromCommand(cmd, setValue, value);

    twai_message_t message;

    message.identifier = frame->id;
    message.extd = frame->isEXT;
    message.rtr = frame->isRTR;
    message.data_length_code = frame->len;
    memcpy(message.data, frame->data, sizeof(message.data)*sizeof(message.data[0]));

    if(!twai_transmit(&message, portMAX_DELAY)) {
        debugSerial.println("ERROR TX");
    }

    delete frame;
}

void DriverSJA1000::handleLoop()
{
    CANDriver::handleLoop();

    if(!canInited)
        return;

    twai_status_info_t status_info;
    twai_get_status_info(&status_info);

    while(status_info.msgs_to_rx > 0) {
        twai_message_t message;

        if (twai_receive(&message, 0) != ESP_OK) {
            return; // no messages
        }

        CanFrame frame;
        frame.id = message.identifier;
        frame.len = message.data_length_code;
        memcpy(frame.data, message.data, sizeof(frame.data)*sizeof(frame.data[0]));
        frame.isRTR = message.rtr;
        frame.isEXT = message.extd;

        onDataRecieved(millis(), frame);

        status_info.msgs_to_rx--;
    }
}

bool DriverSJA1000::setMode(CanDriverMode mode)
{
    stopInterface();

    switch (mode) {
    case CanDriverMode::Normal:
        g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CANConfig->CAN_UART.PIN_TX, (gpio_num_t)CANConfig->CAN_UART.PIN_RX, TWAI_MODE_NORMAL);
        break;

    case CanDriverMode::Loopback:
        g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CANConfig->CAN_UART.PIN_TX, (gpio_num_t)CANConfig->CAN_UART.PIN_RX, TWAI_MODE_NO_ACK);
        break;

    case CanDriverMode::ListenOnly:
        g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CANConfig->CAN_UART.PIN_TX, (gpio_num_t)CANConfig->CAN_UART.PIN_RX, TWAI_MODE_LISTEN_ONLY);
        break;

    default:
        return false;
    }

    debugSerial.printf("CAN-Bus mode %u\n", (uint8_t)mode);

    int result;

    if ((result = twai_driver_install(&g_config, &t_config, &f_config)) != ESP_OK) {
        debugSerial.print("CAN-Bus init failed! E2 - ");
        debugSerial.println(result);
        return false;
    }

    if ((result = twai_start()) != ESP_OK) {
        debugSerial.print("CAN-Bus init failed! E3 - ");
        debugSerial.println(result);
        return false;
    }

    currentMode = mode;
    driverIsRunning = true;

    return true;
}


bool DriverSJA1000::stopInterface()
{
    int result;

    if(driverIsRunning) {
        //Stop the TWAI driver
        if ((result = twai_stop()) != ESP_OK) {
            debugSerial.print("CAN-Bus stop failed! ");
            debugSerial.println(result);
            return false;
        }

        //Uninstall the TWAI driver
        if ((result = twai_driver_uninstall()) != ESP_OK) {
            debugSerial.print("CAN-Bus uninstall failed! ");
            debugSerial.println(result);
            return false;
        }

        driverIsRunning = false;
    }

    return driverIsRunning;
}


DriverSJA1000::~DriverSJA1000()
{
    stopInterface();
}

#endif