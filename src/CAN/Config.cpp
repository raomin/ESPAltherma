#include "Config.hpp"

CAN_Config::~CAN_Config()
{
    if(COMMANDS_LENGTH) {
        for (size_t i = 0; i < COMMANDS_LENGTH; i++) {
            delete COMMANDS[i];
        }
        delete[] COMMANDS;
    }
}