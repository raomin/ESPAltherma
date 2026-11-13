#ifndef ESPALTHERMA_CONFIGSERVICES_H
#define ESPALTHERMA_CONFIGSERVICES_H
#include <string.h>

struct ConfigServiceChanges {bool mqtt,clock;};
inline ConfigServiceChanges configServiceChanges(const AppConfig &before,const AppConfig &after)
{
  bool mqtt=strcmp(before.mqttBase,after.mqttBase)!=0 || strcmp(before.mqttServer,after.mqttServer)!=0 ||
    before.mqttPort!=after.mqttPort || strcmp(before.mqttUser,after.mqttUser)!=0 || strcmp(before.mqttPwd,after.mqttPwd)!=0 ||
    before.mqttTls!=after.mqttTls || before.mqttOff!=after.mqttOff || strcmp(before.mqttClientId,after.mqttClientId)!=0 ||
    before.jsonTable!=after.jsonTable || before.binaryAsNumber!=after.binaryAsNumber || before.debugSerial!=after.debugSerial ||
    before.dhwTarget!=after.dhwTarget || before.lwtTarget!=after.lwtTarget || before.powerLimit!=after.powerLimit;
  return {mqtt,strcmp(before.ntpServer,after.ntpServer)!=0};
}

template<typename StartClock>
inline void configureTimeServers(const char *server,StartClock start)
{
  start(0,0,server[0]?server:"pool.ntp.org",server[0]?nullptr:"time.google.com",nullptr);
}
#endif
