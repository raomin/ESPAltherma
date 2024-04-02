#include "config.hpp"

X10A_Config::~X10A_Config()
{
    if(PARAMETERS_LENGTH) {
        for (size_t i = 0; i < PARAMETERS_LENGTH; i++) {
            delete PARAMETERS[i];
        }
        delete[] PARAMETERS;
    }
}