#ifndef ESPALTHERMA_IMPROV_H
#define ESPALTHERMA_IMPROV_H

// Improv Wi-Fi over serial (https://www.improv-wifi.com/serial/).
// Right after flashing, the web flasher (ESP Web Tools) asks for the WiFi credentials over the USB serial
// port, sends them here, and opens the web interface at the URL we answer.
// Packet: "IMPROV" version(1) type(1) length(1) data(length) checksum(1, sum of the previous bytes)

#include <WiFi.h>

#define IMPROV_VERSION 1
#define IMPROV_TYPE_CURRENT_STATE 0x01
#define IMPROV_TYPE_ERROR_STATE 0x02
#define IMPROV_TYPE_RPC 0x03
#define IMPROV_TYPE_RPC_RESULT 0x04

#define IMPROV_STATE_AUTHORIZED 0x02
#define IMPROV_STATE_PROVISIONING 0x03
#define IMPROV_STATE_PROVISIONED 0x04

#define IMPROV_ERROR_NONE 0x00
#define IMPROV_ERROR_INVALID_RPC 0x01
#define IMPROV_ERROR_UNKNOWN_RPC 0x02
#define IMPROV_ERROR_UNABLE_TO_CONNECT 0x03

#define IMPROV_CMD_WIFI_SETTINGS 0x01
#define IMPROV_CMD_GET_STATE 0x02
#define IMPROV_CMD_GET_INFO 0x03
#define IMPROV_CMD_GET_NETWORKS 0x04

#define IMPROV_CONNECT_TIMEOUT 30000UL

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define IMPROV_CHIP "ESP32-C3"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define IMPROV_CHIP "ESP32-S3"
#else
#define IMPROV_CHIP "ESP32"
#endif

// Defined in netmgr.h
void connectWifi();
bool netOnline();
IPAddress netIP();

class ImprovSerial
{
public:
  ImprovSerial(Stream &port) : _port(port) {}

  void loop()
  {
    while (_port.available())
    {
      feed(_port.read());
    }
    if (_provisioning)
    {
      if (netOnline())
      {
        _provisioning = false;
        sendState(IMPROV_STATE_PROVISIONED);
        sendUrl(IMPROV_CMD_WIFI_SETTINGS);
      }
      else if (millis() - _provisioningStart > IMPROV_CONNECT_TIMEOUT)
      {
        _provisioning = false;
        sendError(IMPROV_ERROR_UNABLE_TO_CONNECT);
        sendState(IMPROV_STATE_AUTHORIZED);
      }
    }
  }

private:
  Stream &_port;
  uint8_t _buf[256];
  size_t _len = 0;
  bool _provisioning = false;
  unsigned long _provisioningStart = 0;

  void feed(uint8_t b)
  {
    static const char header[] = "IMPROV";
    if (_len < 6)
    {
      if (b == (uint8_t)header[_len])
        _buf[_len++] = b;
      else
        _len = b == 'I' ? (_buf[0] = b, 1) : 0;
      return;
    }
    _buf[_len++] = b;
    if (_len >= 9 && _len == (size_t)9 + _buf[8] + 1)
    {
      handlePacket();
      _len = 0;
    }
    else if (_len >= sizeof(_buf))
    {
      _len = 0;
    }
  }

  void handlePacket()
  {
    uint8_t dataLen = _buf[8];
    uint8_t sum = 0;
    for (size_t i = 0; i < (size_t)9 + dataLen; i++)
      sum += _buf[i];
    if (sum != _buf[9 + dataLen] || _buf[6] != IMPROV_VERSION)
    {
      sendError(IMPROV_ERROR_INVALID_RPC);
      return;
    }
    if (_buf[7] != IMPROV_TYPE_RPC || dataLen < 2)
      return;

    const uint8_t *data = _buf + 9;
    uint8_t command = data[0];
    const uint8_t *args = data + 2;
    uint8_t argsLen = data[1];
    switch (command)
    {
    case IMPROV_CMD_WIFI_SETTINGS:
    {
      uint8_t ssidLen = args[0];
      if (argsLen < ssidLen + 2 || ssidLen >= sizeof(config.wifiSsid))
      {
        sendError(IMPROV_ERROR_INVALID_RPC);
        return;
      }
      uint8_t pwdLen = args[1 + ssidLen];
      if (argsLen < ssidLen + 2 + pwdLen || pwdLen >= sizeof(config.wifiPwd))
      {
        sendError(IMPROV_ERROR_INVALID_RPC);
        return;
      }
      memcpy(config.wifiSsid, args + 1, ssidLen);
      config.wifiSsid[ssidLen] = 0;
      memcpy(config.wifiPwd, args + 2 + ssidLen, pwdLen);
      config.wifiPwd[pwdLen] = 0;
      configSave();
      mqttSerial.printf("WiFi credentials received for %s\n", config.wifiSsid);
      sendError(IMPROV_ERROR_NONE);
      sendState(IMPROV_STATE_PROVISIONING);
      _provisioning = true;
      _provisioningStart = millis();
      connectWifi();
      break;
    }
    case IMPROV_CMD_GET_STATE:
      if (netOnline()) // over Ethernet too: the installer then offers to open the device page
      {
        sendState(IMPROV_STATE_PROVISIONED);
        sendUrl(IMPROV_CMD_GET_STATE);
      }
      else
      {
        sendState(_provisioning ? IMPROV_STATE_PROVISIONING : IMPROV_STATE_AUTHORIZED);
      }
      break;
    case IMPROV_CMD_GET_INFO:
    {
      const char *info[] = {"ESPAltherma", ESPALTHERMA_VERSION, IMPROV_CHIP, config.hostname};
      sendResult(IMPROV_CMD_GET_INFO, info, 4);
      break;
    }
    case IMPROV_CMD_GET_NETWORKS:
    {
      WiFi.enableSTA(true); // off on Ethernet boards
      int n = WiFi.scanNetworks(false, false);
      for (int i = 0; i < n; i++)
      {
        String ssid = WiFi.SSID(i);
        char rssi[8];
        snprintf(rssi, sizeof(rssi), "%d", (int)WiFi.RSSI(i));
        const char *net[] = {ssid.c_str(), rssi, WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"};
        sendResult(IMPROV_CMD_GET_NETWORKS, net, 3);
      }
      WiFi.scanDelete();
      sendResult(IMPROV_CMD_GET_NETWORKS, nullptr, 0); // end of the list
      break;
    }
    default:
      sendError(IMPROV_ERROR_UNKNOWN_RPC);
    }
  }

  void send(uint8_t type, const uint8_t *data, uint8_t len)
  {
    uint8_t packet[9 + 255 + 1];
    memcpy(packet, "IMPROV", 6);
    packet[6] = IMPROV_VERSION;
    packet[7] = type;
    packet[8] = len;
    memcpy(packet + 9, data, len);
    uint8_t sum = 0;
    for (size_t i = 0; i < (size_t)9 + len; i++)
      sum += packet[i];
    packet[9 + len] = sum;
    _port.write(packet, 10 + len);
    _port.write('\n');
  }

  void sendState(uint8_t state)
  {
    send(IMPROV_TYPE_CURRENT_STATE, &state, 1);
  }

  void sendError(uint8_t error)
  {
    send(IMPROV_TYPE_ERROR_STATE, &error, 1);
  }

  // RPC result: command, length, then each string prefixed by its length
  void sendResult(uint8_t command, const char *const *strings, int count)
  {
    uint8_t data[255];
    size_t pos = 2;
    for (int i = 0; i < count; i++)
    {
      size_t l = strlen(strings[i]);
      if (pos + 1 + l > sizeof(data))
        break;
      data[pos++] = l;
      memcpy(data + pos, strings[i], l);
      pos += l;
    }
    data[0] = command;
    data[1] = pos - 2;
    send(IMPROV_TYPE_RPC_RESULT, data, pos);
  }

  void sendUrl(uint8_t command)
  {
    String url = "http://" + netIP().toString();
    const char *strings[] = {url.c_str()};
    sendResult(command, strings, 1);
  }
};

ImprovSerial improv(Serial);
#ifdef SECOND_CONSOLE
ImprovSerial improv2(SECOND_CONSOLE);
#endif

#endif
