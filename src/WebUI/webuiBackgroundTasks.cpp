#include "webuiBackgroundTasks.hpp"

X10A_Config* webuiScanX10ARegisterConfig;
CAN_Config* webuiScanCANRegisterConfig;
ValueLoadState valueX10ALoadState = NotLoading;
ValueLoadState valueCANLoadState = NotLoading;
ValueLoadState wifiLoadState = NotLoading;
String valueX10ALoadResponse;
String valueCANLoadResponse;
String wifiLoadResponse;

void webuiScanX10ARegister()
{
  if(valueX10ALoadState == NotLoading || valueX10ALoadState == LoadingFinished) {
    return;
  }

  valueX10ALoadState = Loading;

  bool X10AWasInited = SerialX10A;

  debugSerial.printf("Starting new X10A connection with pins RX: %u, TX: %u\n", webuiScanX10ARegisterConfig->PIN_RX, webuiScanX10ARegisterConfig->PIN_TX);

  x10a_init(webuiScanX10ARegisterConfig, true);

  size_t loadRegistryBufferSize;
  RegistryBuffer *loadRegistryBuffers;

  x10a_initRegistries(&loadRegistryBuffers, loadRegistryBufferSize);

  if (loadRegistryBufferSize == 0) {
    valueX10ALoadState = LoadingFinished;
    debugSerial.println("Given params doesn't contain a registry buffer to fetch");
    return;
  }

  debugSerial.println("Fetching Values");

  x10a_handle(loadRegistryBuffers, loadRegistryBufferSize, false);

  debugSerial.println("Returning Values");

  DynamicJsonDocument resultDoc(webuiScanX10ARegisterConfig->PARAMETERS_LENGTH*JSON_OBJECT_SIZE(2));
  JsonArray obj = resultDoc.to<JsonArray>();

  for (uint8_t i = 0; i < webuiScanX10ARegisterConfig->PARAMETERS_LENGTH; i++) {
    obj.add(webuiScanX10ARegisterConfig->PARAMETERS[i]->asString);
  }

  if(X10AWasInited) {
    debugSerial.println("Restoring original X10A connection");
    x10a_init(config->X10A_CONFIG, true);
  } else {
    x10a_end();
  }

  debugSerial.println("Finished registry scan");

  serializeJson(resultDoc, valueX10ALoadResponse);

  valueX10ALoadState = LoadingFinished;
}

void webuiScanCANRegister()
{
  if(valueCANLoadState == NotLoading || valueCANLoadState == LoadingFinished) {
    return;
  }

  valueCANLoadState = Loading;

  bool CANWasInited = config->CAN_ENABLED;

  debugSerial.printf("Starting new CAN connection with BUS: %i, IC: %i, KBPS: %i\n", (uint8_t)webuiScanCANRegisterConfig->CAN_BUS, (uint8_t)webuiScanCANRegisterConfig->CAN_IC, webuiScanCANRegisterConfig->CAN_SPEED_KBPS);

  if(canBus_setup(webuiScanCANRegisterConfig)) {
    debugSerial.println("Fetching and reading CAN values");
    valueCANLoadResponse = canBus_readAllCommands();
  } else {
    // TODO give webui feedback that can init failed and config needs to be checked.
    debugSerial.println("CAN connection failed!");
  }

  if(CANWasInited) {
    debugSerial.println("Restoring original CAN connection");
    canBus_setup(config->CAN_CONFIG);
  } else {
    canBus_stop();
  }

  debugSerial.println("Finished CAN scan");

  valueCANLoadState = LoadingFinished;
}

void webuiScanWifi()
{
  if(wifiLoadState == NotLoading || wifiLoadState == LoadingFinished) {
    return;
  }

  wifiLoadState = Loading;

  DynamicJsonDocument networksDoc(WIFI_DOC_SIZE);

  scan_wifi();

  for (int16_t i = 0; i < lastWifiScanResultAmount; i++) {
    JsonObject networkDetails = networksDoc.createNestedObject();
    networkDetails["SSID"] = lastWifiScanResults[i]->SSID;
    networkDetails["RSSI"] = lastWifiScanResults[i]->RSSI;
    networkDetails["EncryptionType"] = (lastWifiScanResults[i]->EncryptionType == WIFI_TYPE_OPEN) ? "":"*";
  }

  scan_wifi_delete_result();

  serializeJson(networksDoc, wifiLoadResponse);

  wifiLoadState = LoadingFinished;
}