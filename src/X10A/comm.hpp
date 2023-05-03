#ifndef COMM_H
#define COMM_H

#include <HardwareSerial.h>
#include "ArduinoC.hpp"
#include "X10A.hpp"
#include "debugSerial.hpp"

#define SER_TIMEOUT 300 // leave 300ms for the machine to answer

bool queryRegistry(RegistryBuffer *registryBuffer, X10AProtocol protocol);

#endif