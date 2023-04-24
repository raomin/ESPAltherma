#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "Config/commandDef.hpp"
#include "MQTT/mqtt.hpp"

#define CAN_MESSAGE_TIMEOUT 4 // define timout in seconds for message send to get answer

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
  CommandDef* cmd;
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
    CanFrame* getCanFrameFromCommand(CommandDef* cmd, bool setValue, int value);
    void sniffCAN(uint32_t const, CanFrame const);
    CommandDef* getCommandFromData(const uint8_t *data);
    int HPSU_toSigned(uint16_t value, char* unit);
    virtual bool setMode(CanDriverMode mode);
    void defaultInit();

public:
    virtual bool initInterface();
    virtual void handleLoop();
    virtual void sendCommand(CommandDef *cmd, bool setValue = false, int value = 0);
    void onDataRecieved(uint32_t const, CanFrame const);
    void listenOnly(bool value = true);
    void enableSniffing(bool value);
    void handleMQTTSetRequest(const String &label, const char *payload, const uint32_t length);
};

#endif