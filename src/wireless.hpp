#ifndef WIRELESS_H
#define WIRELESS_H
#include <WiFi.h>
#include <string.h>
#include "debugSerial.hpp"
#include "Config/config.hpp"
#include "restart.hpp"

struct WifiDetails
{
  const String SSID;
  const int32_t RSSI;
  const wifi_auth_mode_t EncryptionType;
};

extern WifiDetails **lastWifiScanResults;
extern int16_t lastWifiScanResultAmount;

void checkWifi();

void start_standalone_wifi();

void setup_wifi();

void scan_wifi_delete_result();

void scan_wifi();

#endif