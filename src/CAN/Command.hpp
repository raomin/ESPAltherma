#ifndef CAN_COMMAND_H
#define CAN_COMMAND_H

#include "ArduinoC.hpp"

#define CAN_COMMAND_NAME_LENGTH 19
#define CAN_COMMAND_LABEL_LENGTH 21
#define CAN_COMMAND_BYTE_LENGTH 7
#define CAN_COMMAND_UNIT_LENGTH 8
#define CAN_COMMAND_TYPE_LENGTH 8

#define CAN_COMMAND_INDEX_NAME 0
#define CAN_COMMAND_INDEX_LABEL 1
#define CAN_COMMAND_INDEX_COMMAND 2
#define CAN_COMMAND_INDEX_ID 3
#define CAN_COMMAND_INDEX_DIVISOR 4
#define CAN_COMMAND_INDEX_WRITABLE 5
#define CAN_COMMAND_INDEX_UNIT 6
#define CAN_COMMAND_INDEX_TYPE 7
#define CAN_COMMAND_INDEX_VALUE_CODE 8

struct CANCommandValueCode
{
    const String key;
    const String value;
    CANCommandValueCode(String keyp, String valuep);
};

struct CANCommand
{
    char name[CAN_COMMAND_NAME_LENGTH];
    char label[CAN_COMMAND_LABEL_LENGTH];
    byte command[CAN_COMMAND_BYTE_LENGTH];
    const uint16_t id;
    const float divisor;
    const bool writable;
    char unit[CAN_COMMAND_UNIT_LENGTH];
    char type[CAN_COMMAND_TYPE_LENGTH];
    const uint8_t valueCodeSize;
    CANCommandValueCode** valueCode;

    CANCommand( const char *namep, const char *labelp, byte commandp[CAN_COMMAND_BYTE_LENGTH], uint16_t idp, float divisorp, bool writablep, const char *unitp, const char *typep, const uint8_t valueCodeSizep, CANCommandValueCode** valueCodep);
    ~CANCommand();
};

#endif