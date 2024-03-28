#ifndef WIRELESS_H
#define WIRELESS_H


#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <string.h>
#include "debugSerial.hpp"
#include "Config/config.hpp"
#include "restart.hpp"


#ifdef ARDUINO_ARCH_ESP8266
#define ENCRYPTION_TYPE uint8_t
#else
#define ENCRYPTION_TYPE wifi_auth_mode_t
#endif

struct WifiDetails
{
  const String SSID;
  const int32_t RSSI;
  const ENCRYPTION_TYPE EncryptionType;
};

extern WifiDetails **lastWifiScanResults;
extern int16_t lastWifiScanResultAmount;

void checkWifi();

void start_standalone_wifi();

void setup_wifi();

void scan_wifi_delete_result();

void scan_wifi();

#ifdef ARDUINO_ARCH_ESP8266
void get_wifi_bssid(const char *ssid, uint8_t *bssid, uint32_t *wifi_channel);
#endif

#endif