#pragma once

#include <Arduino.h>

#define MAX_BUFFER_SIZE 32 //max bytes returned from X10A Port

struct RegistryBuffer {
  char RegistryID;
  byte Buffer[MAX_BUFFER_SIZE];
  bool Success;
  char CRC;
};