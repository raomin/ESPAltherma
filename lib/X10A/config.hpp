#pragma once

#include <Arduino.h>
#include "parameterDef.hpp"

enum class X10AProtocol : uint8_t {
    I,
    S
};

struct X10A_Config
{
    uint8_t PIN_RX;
    uint8_t PIN_TX;
    uint32_t FREQUENCY;
    X10AProtocol X10A_PROTOCOL;
    size_t PARAMETERS_LENGTH;
    ParameterDef** PARAMETERS;
    String WEBUI_SELECTION_VALUES;

    ~X10A_Config();
};