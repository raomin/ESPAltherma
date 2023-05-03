#include "webuiBackgroundTasks.hpp"

WebUIScanRegister webuiScanRegisterConfig;
ValueLoadState valueLoadState = NotLoading;
ValueLoadState wifiLoadState = NotLoading;
String valueLoadResponse;
String wifiLoadResponse;

void webuiScanRegister()
{
  if(valueLoadState == NotLoading || valueLoadState == LoadingFinished)
  {
    return;
  }

  valueLoadState = Loading;

  bool serialX10AWasInited = SerialX10A;

  debugSerial.printf("Starting new serial connection with pins RX: %u, TX: %u\n", webuiScanRegisterConfig.PinRx, webuiScanRegisterConfig.PinTx);
  debugSerial.println("Waiting for registry scan to finish...");

  debugSerial.println("Starting registry scan...");

  X10AInit(webuiScanRegisterConfig.PinRx, webuiScanRegisterConfig.PinTx);

  DynamicJsonDocument modelsDoc(MODELS_DOC_SIZE);
  deserializeJson(modelsDoc, webuiScanRegisterConfig.Params);
  JsonArray modelsDocArr = modelsDoc.as<JsonArray>();

  debugSerial.printf("Creating labelDefs %i\n", modelsDocArr.size());

  size_t labelsSize = modelsDocArr.size();
  ParameterDef **labelsToLoad = new ParameterDef*[labelsSize];

  uint8_t counter = 0;
  for (JsonArray model : modelsDocArr)
  {
    labelsToLoad[counter] = new ParameterDef(model[0], model[1], model[2], model[3], model[4], model[5]);
    counter++;
  }

  size_t loadRegistryBufferSize;
  RegistryBuffer *loadRegistryBuffers;

  initRegistries(&loadRegistryBuffers, loadRegistryBufferSize, labelsToLoad, labelsSize);

  if (loadRegistryBufferSize == 0)
  {
    valueLoadState = LoadingFinished;
    debugSerial.println("Given params doesn't contain a registry buffer to fetch");
    return;
  }

  debugSerial.println("Fetching Values");

  handleX10A(loadRegistryBuffers, loadRegistryBufferSize, labelsToLoad, labelsSize, false, webuiScanRegisterConfig.protocol);

  debugSerial.println("Returning Values");

  DynamicJsonDocument resultDoc(labelsSize*JSON_OBJECT_SIZE(2));
  JsonArray obj = resultDoc.to<JsonArray>();

  for (uint8_t i = 0; i < labelsSize; i++) {
    obj.add(labelsToLoad[i]->asString);
  }

  for (size_t i = 0; i < labelsSize; i++)
  {
      delete labelsToLoad[i];
  }
  delete[] labelsToLoad;

  if(serialX10AWasInited)
  {
    debugSerial.println("Restoring original X10A connection");
    X10AInit(config->PIN_RX, config->PIN_TX);
  }
  else
  {
    X10AEnd();
  }

  debugSerial.println("Finished registry scan");

  serializeJson(resultDoc, valueLoadResponse);

  valueLoadState = LoadingFinished;
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