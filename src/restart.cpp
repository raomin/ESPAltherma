#include "restart.hpp"

void restart_board()
{
  #ifdef ARDUINO_ARCH_ESP8266
  ESP.restart();
  #else
  esp_restart();
  #endif
}