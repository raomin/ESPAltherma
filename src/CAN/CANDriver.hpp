#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "CANCommand.hpp"
#include "MQTT/mqtt.hpp"

#define CAN_MESSAGE_TIMEOUT 4 // define timout in seconds for message send to get answer
#define NOT_IMPLEMENTED = 0

struct CanFrame
{
  uint32_t id;
  uint8_t  data[8];
  uint8_t  len;
  bool     isRTR;
  bool     isEXT;
};

struct CMDSendInfo
{
  bool pending = false;
  uint64_t timeMessageSend = 0;
  CANCommand* cmd;
};

enum class CanDriverMode : uint8_t
{
  Normal,
  Loopback,
  ListenOnly
};

class CANDriver
{
protected:
    CMDSendInfo** cmdSendInfos;
    bool canInited = false;
    ulong lastTimeRunned = 0;
    bool sniffingEnabled = false;
    CanDriverMode currentMode;
    const CAN_Config* CANConfig;
    bool CANDataToString = false;
    size_t currentDataToStringIndex;

    CanFrame* getCanFrameFromCommand(CANCommand* cmd, bool setValue, int value);
    void sniffCAN(uint32_t const, CanFrame const);
    CANCommand* getCommandFromData(const uint8_t *data);
    int HPSU_toSigned(uint16_t value, char* unit);
    virtual bool setMode(CanDriverMode mode) NOT_IMPLEMENTED;
    void defaultInit();
    void checkPendingMessages();

public:
    CANDriver(const CAN_Config* CANConfig);
    virtual bool initInterface() NOT_IMPLEMENTED;
    virtual void handleLoop();
    virtual String readAllCommands();
    virtual void sendCommand(CANCommand *cmd, bool setValue = false, int value = 0) NOT_IMPLEMENTED;
    void onDataRecieved(uint32_t const, CanFrame const);
    void listenOnly(bool value = true);
    void enableSniffing(bool value);
    void handleMQTTSetRequest(const String &label, const char *payload, const uint32_t length);
    virtual ~CANDriver();
};

#endif