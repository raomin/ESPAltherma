#ifndef COMMANDDEF_H
#define COMMANDDEF_H

#include "ArduinoC.hpp"

#define COMMAND_NAME_LENGTH 19
#define COMMAND_LABEL_LENGTH 21
#define COMMAND_BYTE_LENGTH 7
#define COMMAND_UNIT_LENGTH 8
#define COMMAND_TYPE_LENGTH 8

#define COMMANDDEF_INDEX_NAME 0
#define COMMANDDEF_INDEX_LABEL 1
#define COMMANDDEF_INDEX_COMMAND 2
#define COMMANDDEF_INDEX_ID 3
#define COMMANDDEF_INDEX_DIVISOR 4
#define COMMANDDEF_INDEX_WRITABLE 5
#define COMMANDDEF_INDEX_UNIT 6
#define COMMANDDEF_INDEX_TYPE 7
#define COMMANDDEF_INDEX_VALUE_CODE 8

struct CommandDefValueCode
{
    const String key;
    const String value;
    CommandDefValueCode(String keyp, String valuep);
};

struct CommandDef
{
    char name[COMMAND_NAME_LENGTH];
    char label[COMMAND_LABEL_LENGTH];
    byte command[COMMAND_BYTE_LENGTH];
    const uint16_t id;
    const float divisor;
    const bool writable;
    char unit[COMMAND_UNIT_LENGTH];
    char type[COMMAND_TYPE_LENGTH];
    const uint8_t valueCodeSize;
    CommandDefValueCode** valueCode;

    CommandDef( const char *namep, const char *labelp, byte commandp[COMMAND_BYTE_LENGTH], uint16_t idp, float divisorp, bool writablep, const char *unitp, const char *typep, const uint8_t valueCodeSizep, CommandDefValueCode** valueCodep);
    ~CommandDef();
};

#endif