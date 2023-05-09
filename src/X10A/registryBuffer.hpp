#ifndef REGISTRY_BUFFER_H
#define REGISTRY_BUFFER_H

#include "arduinoC.hpp"

#define MAX_BUFFER_SIZE 32 //max bytes returned from X10A Port

struct RegistryBuffer {
  char RegistryID;
  byte Buffer[MAX_BUFFER_SIZE];
  bool Success;
  char CRC;
};

#endif