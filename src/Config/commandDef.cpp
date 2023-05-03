#include "commandDef.hpp"

CommandDefValueCode::CommandDefValueCode(String keyp, String valuep) : key(keyp), value(valuep)
{

};

CommandDef::CommandDef( const char *namep, const char *labelp, byte commandp[COMMAND_BYTE_LENGTH], uint16_t idp, float divisorp, bool writablep, const char *unitp, const char *typep, const uint8_t valueCodeSizep, CommandDefValueCode** valueCodep) : id(idp), divisor(divisorp), writable(writablep), valueCodeSize(valueCodeSizep)
{
    uint8_t i;

    for(i = 0; i < COMMAND_BYTE_LENGTH; i++)
        command[i] = commandp[i];

    strcpy(name, namep);
    strcpy(label, labelp);
    strcpy(unit, unitp);
    strcpy(type, typep);

    valueCode = valueCodep;
};

CommandDef::~CommandDef()
{
    if(valueCodeSize)
    {
        for (size_t i = 0; i < valueCodeSize; i++)
        {
            delete valueCode[i];
        }
        delete[] valueCode;
    }
}