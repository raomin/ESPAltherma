#include "webuiBackgroundTasks.hpp"

WebUIScanX10ARegister webuiScanX10ARegisterConfig;
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

  bool serialX10AWasInited = SerialX10A;

  debugSerial.printf("Starting new serial connection with pins RX: %u, TX: %u\n", webuiScanX10ARegisterConfig.PinRx, webuiScanX10ARegisterConfig.PinTx);
  debugSerial.println("Starting registry scan...");

  X10AInit(webuiScanX10ARegisterConfig.PinRx, webuiScanX10ARegisterConfig.PinTx);

  DynamicJsonDocument modelsDoc(MODELS_DOC_SIZE);
  deserializeJson(modelsDoc, webuiScanX10ARegisterConfig.Params);
  JsonArray modelsDocArr = modelsDoc.as<JsonArray>();

  debugSerial.printf("Creating labelDefs %i\n", modelsDocArr.size());

  size_t labelsSize = modelsDocArr.size();
  ParameterDef **labelsToLoad = new ParameterDef*[labelsSize];

  uint8_t counter = 0;
  for (JsonArray model : modelsDocArr) {
    labelsToLoad[counter] = new ParameterDef(model[0], model[1], model[2], model[3], model[4], model[5]);
    counter++;
  }

  size_t loadRegistryBufferSize;
  RegistryBuffer *loadRegistryBuffers;

  initRegistries(&loadRegistryBuffers, loadRegistryBufferSize, labelsToLoad, labelsSize);

  if (loadRegistryBufferSize == 0) {
    valueX10ALoadState = LoadingFinished;
    debugSerial.println("Given params doesn't contain a registry buffer to fetch");
    return;
  }

  debugSerial.println("Fetching Values");

  handleX10A(loadRegistryBuffers, loadRegistryBufferSize, labelsToLoad, labelsSize, false, webuiScanX10ARegisterConfig.Protocol);

  debugSerial.println("Returning Values");

  DynamicJsonDocument resultDoc(labelsSize*JSON_OBJECT_SIZE(2));
  JsonArray obj = resultDoc.to<JsonArray>();

  for (uint8_t i = 0; i < labelsSize; i++) {
    obj.add(labelsToLoad[i]->asString);
  }

  for (size_t i = 0; i < labelsSize; i++) {
      delete labelsToLoad[i];
  }
  delete[] labelsToLoad;

  if(serialX10AWasInited) {
    debugSerial.println("Restoring original X10A connection");
    X10AInit(config->PIN_RX, config->PIN_TX);
  } else {
    X10AEnd();
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
  debugSerial.println("Starting registry scan...");

  canBus_setup(webuiScanCANRegisterConfig);

  debugSerial.println("Fetching and reading values");

  valueCANLoadResponse = canBus_readAllCommands();

  if(CANWasInited) {
    debugSerial.println("Restoring original CAN connection");
    canBus_setup(config->CAN_CONFIG);
  } else {
    canBus_stop();
  }

  debugSerial.println("Finished registry scan");

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
    networkDetails["EncryptionType"] = (lastWifiScanResults[i]->EncryptionType == WIFI_AUTH_OPEN) ? "":"*";
  }

  scan_wifi_delete_result();

  serializeJson(networksDoc, wifiLoadResponse);

  wifiLoadState = LoadingFinished;
}