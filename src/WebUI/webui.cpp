#include "webui.hpp"

String lastUploadFileName;
bool webOTAIsBusy = false;
bool updateVersionValid = false;
LoopRunStatus mainLoopStatus = LoopRunStatus::Running;

// Set web server port number to 80
AsyncWebServer server(80);

bool formatDefaultFS()
{
  LittleFS.end();
  bool result = LittleFS.format();

  if(!result)
    return false;

  LittleFS.begin();
  File file = LittleFS.open(MODELS_FILE, FILE_WRITE, true);
  file.print("[]");
  file.close();

  file = LittleFS.open(CAN_COMMANDS_FILE, FILE_WRITE, true);
  file.print("[]");
  file.close();

  resetPersistence();

  return true;
}

void onWifiLoadNetworks(AsyncWebServerRequest *request)
{
  if(wifiLoadState != NotLoading) {
    request->send(202, "text/plain", "Wifi scan in progress");
    return;
  }

  wifiLoadState = Pending;

  request->send(200, "application/json", "OK");
}

void onWifiLoadFinished(AsyncWebServerRequest *request)
{
  if(wifiLoadState == NotLoading) {
    request->send(503, "text/plain", "No scan in progress");
    return;
  }

  if(wifiLoadState == Loading || wifiLoadState == Pending) {
    request->send(503, "text/plain", "Wifi scan not finished");
    return;
  }

  request->send(200, "application/json", wifiLoadResponse);
  wifiLoadResponse = "";

  wifiLoadState = NotLoading;
}

void onLoadBoardInfo(AsyncWebServerRequest *request)
{
  String response = "{";
  response.concat(JSON_BOARD_DEFAULTS);

  esp_app_desc_t app_info;
  esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info);

  SemanticVersion version(app_info.version);

  response += ",\"Version\": \"";

  char versionText[48] = "";

  if(version.hasVersionNr) {
    snprintf(versionText + strlen(versionText), sizeof(versionText) - strlen(versionText), "%i.%i.%i", version.Major, version.Minor, version.Patch);

    if(version.hasBuildNr || version.hasCommitNr) {
      strncat(versionText, "-", sizeof(versionText) - strlen(versionText));
    }
  }

  if(version.hasBuildNr) {
    snprintf(versionText + strlen(versionText), sizeof(versionText) - strlen(versionText), "%i-", version.Build);

    if(version.hasCommitNr) {
      strncat(versionText, "-", sizeof(versionText) - strlen(versionText));
    }
  }

  if(version.hasCommitNr) {
    snprintf(versionText + strlen(versionText), sizeof(versionText) - strlen(versionText), "%s", version.Extra);
  }

  response.concat(versionText);
  response += "\"}";

  request->send(200, "application/json", response);
}

void onIndex(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", indexHTML_start, indexHTML_end - indexHTML_start);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void onRequestPicoCSS(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", picoCSS_start, picoCSS_end - picoCSS_start);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void onRequestMainCSS(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", mainCSS_start, mainCSS_end - mainCSS_start);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void onRequestMainJS(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", mainJS_start, mainJS_end - mainJS_start);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void onRequestMD5JS(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", md5JS_start, md5JS_end - md5JS_start);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void onFormat(AsyncWebServerRequest *request)
{
  bool result = formatDefaultFS();

  request->onDisconnect([]()
  {
    restart_board();
  });

  request->send(200, "text/javascript", String(result ? "OK" : "FAILED"));
}

void onLoadModels(AsyncWebServerRequest *request)
{
  request->send(LittleFS, MODELS_FILE, "text/json");
}

void onLoadCommands(AsyncWebServerRequest *request)
{
  request->send(LittleFS, CAN_COMMANDS_FILE, "text/json");
}

void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t* data, size_t len, bool final)
{
  String fsFilename;

  if (!index)
  {
    do
    {
      fsFilename = "/P" + String(millis()) + ".json";
    } while (LittleFS.exists(fsFilename));

    // open the file on first call and store the file handle in the request object
    request->_tempFile = LittleFS.open(fsFilename, "w");
    debugSerial.printf("Upload Start: %s\n", filename.c_str());
  }

  if (len)
  {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    debugSerial.printf("Writing file: %s index=%i len=%i\n", filename.c_str(), index, len);
  }

  if (final)
  {
    lastUploadFileName = "/" + String(request->_tempFile.name());
    debugSerial.printf("Upload Complete: %s, size: %i\n", filename.c_str(), index + len);

    // close the file handle as the upload is now done
    request->_tempFile.close();

    // minimize json file
    File modelsFile = LittleFS.open(lastUploadFileName, FILE_READ);
    DynamicJsonDocument modelsDoc(MODEL_DEFINITION_UPLOAD_SIZE);
    deserializeJson(modelsDoc, modelsFile);
    modelsFile.close();

    modelsFile = LittleFS.open(lastUploadFileName, FILE_WRITE);
    serializeJson(modelsDoc, modelsFile);
    size_t newFileSize = modelsFile.size();
    modelsFile.close();

    debugSerial.printf("JSON Minify Complete: %s, new size: %i\n", lastUploadFileName.c_str(), newFileSize);
  }
}

void onUploadX10AFile(AsyncWebServerRequest *request)
{
  if(!request->hasParam("file", true, true))
  {
    request->send(422, "text/plain", "Missing X10A file");
    return;
  }

  String fsFilename = lastUploadFileName;
  debugSerial.printf("Found LittleFS Filename: %s\n", fsFilename.c_str());

  File modelsFile = LittleFS.open(MODELS_FILE, FILE_READ);
  DynamicJsonDocument modelsDoc(MODELS_DOC_SIZE);
  deserializeJson(modelsDoc, modelsFile);
  JsonArray modelsDocArr = modelsDoc.as<JsonArray>();
  modelsFile.close();

  File uploadFileFS = LittleFS.open(fsFilename, FILE_READ);
  DynamicJsonDocument uploadDoc(MODEL_DEFINITION_DOC_SIZE);
  deserializeJson(uploadDoc, uploadFileFS);
  uploadFileFS.close();

  bool newModel = true;
  for (JsonObject model : modelsDocArr)
  {
    if(strcmp(model["Model"].as<const char*>(), uploadDoc["Model"].as<const char*>()) == 0)
    {
      debugSerial.printf("Found existing Model: %s\n", model["Model"].as<const char*>());

      newModel = false;

      bool existingLanguage = false;
      for (JsonPair kv : model["Files"].as<JsonObject>())
      {
        if(strcmp(kv.key().c_str(), uploadDoc["Language"].as<const char*>()) == 0)
        {
          debugSerial.printf("Found existing Model file: %s\n", kv.key().c_str());
          fsFilename = kv.value().as<const char*>();
          existingLanguage = true;
          break;
        }
      }

      if(!existingLanguage)
      {
        debugSerial.printf("add new language to existing Model file: %s\n", uploadDoc["Language"].as<const char*>());
        model["Files"][uploadDoc["Language"].as<const char *>()] = fsFilename;
      }
    }
  }

  if(newModel)
  {
    debugSerial.printf("Found new Model: %s\n", uploadDoc["Model"].as<const char*>());

    JsonObject newModelObect = modelsDocArr.createNestedObject();
    newModelObect["Model"] = uploadDoc["Model"].as<const char*>();
    newModelObect["Files"][uploadDoc["Language"].as<const char *>()] = fsFilename;
  }

  serializeJson(modelsDoc, Serial);

  modelsFile = LittleFS.open(MODELS_FILE, FILE_WRITE);
  serializeJson(modelsDoc, modelsFile);
  modelsFile.close();

  request->send(200);
}

void onUploadConfigFile(AsyncWebServerRequest *request)
{
  if(!request->hasParam("file", true, true))
  {
    request->send(422, "text/plain", "Missing config file");
    return;
  }

  String fsFilename = lastUploadFileName;
  debugSerial.printf("Found LittleFS Filename: %s\n", fsFilename.c_str());

  if(LittleFS.exists(CONFIG_FILE))
  {
    LittleFS.remove(CONFIG_FILE);
  }

  LittleFS.rename(lastUploadFileName, CONFIG_FILE);

  request->onDisconnect([]()
  {
    restart_board();
  });

  request->send(200);
}

void onLoadValuesResult(AsyncWebServerRequest *request)
{
  if(valueX10ALoadState == NotLoading)
  {
    request->send(503, "text/plain", "No values loading in progress");
    return;
  }

  if(valueX10ALoadState == Loading || valueX10ALoadState == Pending)
  {
    request->send(503, "text/plain", "Values loading not finished");
    return;
  }

  request->send(200, "application/json", valueX10ALoadResponse);
  valueX10ALoadResponse = "";

  valueX10ALoadState = NotLoading;
}

void onLoadValues(AsyncWebServerRequest *request)
{
  if(!request->hasParam("PIN_RX", true) || !request->hasParam("PIN_TX", true) || !request->hasParam("PARAMS", true))
  {
    request->send(422, "text/plain", "Missing parameters PIN_RX, PIN_TX or PARAMS");
    return;
  }

  if(valueX10ALoadState != NotLoading)
  {
    request->send(202, "text/plain", "Value loading in progress");
    return;
  }

  webuiScanX10ARegisterConfig.PinRx = request->getParam("PIN_RX", true)->value().toInt();
  webuiScanX10ARegisterConfig.PinTx = request->getParam("PIN_TX", true)->value().toInt();
  webuiScanX10ARegisterConfig.Protocol = (X10AProtocol)request->getParam("X10A_PROTOCOL", true)->value().toInt();
  webuiScanX10ARegisterConfig.Params = request->getParam("PARAMS", true)->value();

  valueX10ALoadState = Pending;

  request->send(200, "application/json", "OK");
}

void onLoadModel(AsyncWebServerRequest *request)
{
  if(!request->hasParam("modelFile", true))
  {
    request->send(422, "text/plain", "Missing model file");
    return;
  }

  String modelFile = request->getParam("modelFile", true)->value();

  debugSerial.print("Found model file: ");
  debugSerial.println(modelFile);

  if(!LittleFS.exists(modelFile))
  {
    request->send(400, "text/plain", "Model file not found");
    return;
  }

  request->send(LittleFS, modelFile, "text/json");
}

void onLoadCommand(AsyncWebServerRequest *request)
{
  if(!request->hasParam("commandFile", true))
  {
    request->send(422, "text/plain", "Missing command file");
    return;
  }

  String commandFile = request->getParam("commandFile", true)->value();

  debugSerial.print("Found command file: ");
  debugSerial.println(commandFile);

  if(!LittleFS.exists(commandFile))
  {
    request->send(400, "text/plain", "Command file not found");
    return;
  }

  request->send(LittleFS, commandFile, "text/json");
}

void onLoadConfig(AsyncWebServerRequest *request)
{
  if(!LittleFS.exists(CONFIG_FILE))
  {
    request->send(200, "text/json", "{}");
    return;
  }

  File configFile = LittleFS.open(CONFIG_FILE, FILE_READ);
  size_t configFileSize = configFile.size();
  configFile.close();

  if(configFileSize == 0)
  {
    request->send(200, "text/json", "{}");
    return;
  }

  request->send(LittleFS, CONFIG_FILE, "text/json");
}

void onExportConfig(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response;

  if(!LittleFS.exists(CONFIG_FILE))
  {
    response = request->beginResponse(200, "text/json", "{}");
    response->addHeader("Content-Disposition", "attachment; filename=\"config.json\"");
    response->addHeader("Content-Length", "2");
    request->send(response);
    return;
  }

  File file = LittleFS.open(CONFIG_FILE, "r");
  size_t filesize = file.size();
  file.close();

  response = request->beginResponse(LittleFS, CONFIG_FILE, "text/json", true);
  response->addHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  response->addHeader("Content-Length", String(filesize));
  request->send(response);
}

bool handleCAN(AsyncWebServerRequest *request, CAN_Config** CANConfigPointer)
{
  if(!request->hasParam("can_ic_type", true) || !request->hasParam("can_speed_kbps", true)) {
    request->send(422, "text/plain", "Missing parameter(s) for CAN-Bus IC Type or CAN-Speed");
    return false;
  }

  CAN_ICBus canICBus = CAN_ICBus::None;
  CAN_ICTypes canICTypes = CAN_ICTypes::None;

  String ICType = request->getParam("can_ic_type", true)->value();

  if(ICType.startsWith("uart_")) {
    canICBus = CAN_ICBus::UART;
  } else if(ICType.startsWith("spi_")) {
    canICBus = CAN_ICBus::SPI;
  } else if(ICType.startsWith("bt_")) {
    canICBus = CAN_ICBus::BT;
  } else {
    request->send(422, "text/plain", "Invalid CAN IC/Chip communication type given");
    return false;
  }

  ICType = ICType.substring(ICType.indexOf('_') + 1);

  if(ICType == "mcp2515") {
    canICTypes = CAN_ICTypes::MCP2515;
  } else if(ICType == "elm327") {
    canICTypes = CAN_ICTypes::ELM327;
  } else if(ICType == "sja1000") {
    canICTypes = CAN_ICTypes::SJA1000;
  } else {
    request->send(422, "text/plain", "Invalid CAN IC/Chip type given");
    return false;
  }

  if(canICBus == CAN_ICBus::UART &&
    (!request->hasParam("pin_can_uart_rx", true) || !request->hasParam("pin_can_uart_tx", true)))
  {
    request->send(422, "text/plain", "Missing parameter(s) for CAN-Bus UART");
    return false;
  }

  if(canICBus == CAN_ICBus::SPI &&
     (!request->hasParam("pin_can_spi_mosi", true) ||
      !request->hasParam("pin_can_spi_miso", true) ||
      !request->hasParam("pin_can_spi_cs", true) ||
      !request->hasParam("pin_can_spi_sck", true) ||
      !request->hasParam("pin_can_spi_int", true) ||
      !request->hasParam("pin_can_spi_mhz", true)))
  {
    request->send(422, "text/plain", "Missing parameter(s) for CAN-Bus SPI");
    return false;
  }

  if(canICBus == CAN_ICBus::BT &&
     (!request->hasParam("pin_can_bt_devicename", true) ||
      (request->hasParam("pin_can_bt_use_pin", true) && !request->hasParam("pin_can_bt_pin", true))))
  {
    request->send(422, "text/plain", "Missing parameter(s) for CAN-Bus Bluetooth");
    return false;
  }

  CAN_Config* CANConfig = new CAN_Config();
  CANConfig->CAN_IC = canICTypes;
  CANConfig->CAN_BUS = canICBus;

  *CANConfigPointer = CANConfig;

  if(CANConfig->CAN_BUS == CAN_ICBus::SPI) {
    CANConfig->CAN_SPI.PIN_MISO = request->getParam("pin_can_spi_miso", true)->value().toInt();
    CANConfig->CAN_SPI.PIN_MOSI = request->getParam("pin_can_spi_mosi", true)->value().toInt();
    CANConfig->CAN_SPI.PIN_SCK = request->getParam("pin_can_spi_sck", true)->value().toInt();
    CANConfig->CAN_SPI.PIN_CS = request->getParam("pin_can_spi_cs", true)->value().toInt();
    CANConfig->CAN_SPI.PIN_INT = request->getParam("pin_can_spi_int", true)->value().toInt();
    CANConfig->CAN_SPI.IC_MHZ = request->getParam("pin_can_spi_mhz", true)->value().toInt();
  } else if(CANConfig->CAN_BUS == CAN_ICBus::UART) {
    CANConfig->CAN_UART.PIN_RX = request->getParam("pin_can_uart_rx", true)->value().toInt();
    CANConfig->CAN_UART.PIN_TX = request->getParam("pin_can_uart_tx", true)->value().toInt();
  } else if(CANConfig->CAN_BUS == CAN_ICBus::BT) {
    CANConfig->CAN_BLUETOOTH.DEVICENAME = (char *)request->getParam("pin_can_bt_devicename", true)->value().c_str();
    CANConfig->CAN_BLUETOOTH.USE_PIN = request->hasParam("pin_can_bt_use_pin", true);
    if(CANConfig->CAN_BLUETOOTH.USE_PIN) {
      CANConfig->CAN_BLUETOOTH.PIN = (char *)request->getParam("pin_can_bt_pin", true)->value().c_str();
    }
  }

  CANConfig->CAN_SPEED_KBPS = request->getParam("can_speed_kbps", true)->value().toInt();
  CANConfig->CAN_MQTT_TOPIC_NAME = (char *)request->getParam("can_mqtt_topic_name", true)->value().c_str();
  CANConfig->CAN_READONLY_ENABLED = request->hasParam("can_readonly_enabled", true);
  CANConfig->CAN_SNIFFING_ENABLED = request->hasParam("can_sniffing_enabled", true);
  CANConfig->CAN_AUTOPOLL_MODE = (CAN_PollMode)request->getParam("can_autopoll_mode", true)->value().toInt();

  if(CANConfig->CAN_AUTOPOLL_MODE == CAN_PollMode::Auto) {
    CANConfig->CAN_AUTOPOLL_TIME = request->getParam("can_autopoll_time", true)->value().toInt();
  }

  if(request->hasParam("definedCommands", true)) {
    DynamicJsonDocument jsonCommands(MODELS_CONFIG_SIZE);
    deserializeJson(jsonCommands, request->getParam("definedCommands", true)->value());
    JsonArray commandsArray = jsonCommands.as<JsonArray>();

    CANConfig->COMMANDS_LENGTH = commandsArray.size();
    CANConfig->COMMANDS = new CANCommand*[CANConfig->COMMANDS_LENGTH];

    int counter = 0;
    for (JsonArray value : commandsArray) {
      JsonArray commandBytes = value[CAN_COMMAND_INDEX_COMMAND];
      byte commandArray[] = {
          commandBytes[0].as<const byte>(),
          commandBytes[1].as<const byte>(),
          commandBytes[2].as<const byte>(),
          commandBytes[3].as<const byte>(),
          commandBytes[4].as<const byte>(),
          commandBytes[5].as<const byte>(),
          commandBytes[6].as<const byte>()
      };

      CANCommandValueCode** valueCodes;
      uint8_t valueCodeSize = 0;

      if(value.size() > CAN_COMMAND_INDEX_VALUE_CODE) {
        JsonObject valueCodeCommands = value[CAN_COMMAND_INDEX_VALUE_CODE].as<JsonObject>();
        valueCodeSize = valueCodeCommands.size();
        valueCodes = new CANCommandValueCode*[valueCodeSize];

        uint8_t valueCodeCounter = 0;

        for (JsonPair keyValue : valueCodeCommands) {
          valueCodes[valueCodeCounter] = new CANCommandValueCode(keyValue.key().c_str(), keyValue.value().as<String>());
          valueCodeCounter++;
        }
      } else {
        valueCodes = nullptr;
      }

      CANConfig->COMMANDS[counter] = new CANCommand(
        value[CAN_COMMAND_INDEX_NAME],
        value[CAN_COMMAND_INDEX_LABEL],
        commandArray,
        value[CAN_COMMAND_INDEX_ID].as<const uint16_t>(),
        value[CAN_COMMAND_INDEX_DIVISOR].as<const float>(),
        value[CAN_COMMAND_INDEX_WRITABLE].as<const bool>(),
        value[CAN_COMMAND_INDEX_UNIT],
        value[CAN_COMMAND_INDEX_TYPE],
        valueCodeSize,
        valueCodes);

      counter++;
    }
  } else {
    CANConfig->COMMANDS_LENGTH = 0;
    CANConfig->COMMANDS = nullptr;
  }

  return true;
}

void onSaveConfig(AsyncWebServerRequest *request)
{
  if(!request->hasParam("standalone_wifi", true))
  {
    if(!request->hasParam("ssid", true) || !request->hasParam("ssid_password", true))
    {
      request->send(422, "text/plain", "Missing parameter(s) for ssid!");
      return;
    }

    if(request->hasParam("ssid_staticip", true) && (!request->hasParam("ssid_ip", true) ||
                                                    !request->hasParam("ssid_subnet", true) ||
                                                    !request->hasParam("ssid_gateway", true) ||
                                                    !request->hasParam("primary_dns", true) ||
                                                    !request->hasParam("secondary_dns", true)))
    {
      request->send(422, "text/plain", "Missing parameter(s) for static ip");
      return;
    }
  }

  if(!request->hasParam("mqtt_server", true) || !request->hasParam("mqtt_username", true) || !request->hasParam("mqtt_password", true) || !request->hasParam("mqtt_topic_name", true) || !request->hasParam("mqtt_port", true) || !request->hasParam("frequency", true))
  {
    request->send(422, "text/plain", "Missing parameter(s) for MQTT!");
    return;
  }

  if(request->hasParam("mqtt_use_onetopic", true) && !request->hasParam("mqtt_onetopic_name", true))
  {
    request->send(422, "text/plain", "Missing parameter(s) for MQTT onetopic");
    return;
  }

  if(!request->hasParam("pin_enable_config", true))
  {
    request->send(422, "text/plain", "Missing parameter pin to enable config");
    return;
  }

  if(request->hasParam("x10a_enabled", true) && (!request->hasParam("pin_rx", true) || !request->hasParam("pin_tx", true) || !request->hasParam("x10a_protocol", true)))
  {
    request->send(422, "text/plain", "Missing parameter(s) for X10A");
    return;
  }

  if(request->hasParam("heating_enabled", true) && !request->hasParam("pin_heating", true))
  {
    request->send(422, "text/plain", "Missing parameter(s) for heating");
    return;
  }

  if(request->hasParam("cooling_enabled", true) && !request->hasParam("pin_cooling", true))
  {
    request->send(422, "text/plain", "Missing parameter(s) for cooling");
    return;
  }

  if(request->hasParam("sg_enabled", true) && (!request->hasParam("pin_sg1", true) || !request->hasParam("pin_sg2", true)))
  {
    request->send(422, "text/plain", "Missing parameter(s) for SmartGrid");
    return;
  }

  CAN_Config* CANConfig = nullptr;
  if(request->hasParam("can_enabled", true) && !handleCAN(request, &CANConfig)) {
    return;
  }

  // ensure main loop stopped and nothing trys to access config instance as this will be destroyed now
  mainLoopStatus = LoopRunStatus::Stopping;

  while(mainLoopStatus != LoopRunStatus::Stopped)
    delay(10);

  if(config)
    delete config;

  config = new Config();
  config->configStored = true;
  config->STANDALONE_WIFI = request->hasParam("standalone_wifi", true);

  if(!config->STANDALONE_WIFI)
  {
    config->SSID = (char *)request->getParam("ssid", true)->value().c_str();
    config->SSID_PASSWORD = (char *)request->getParam("ssid_password", true)->value().c_str();

    config->SSID_STATIC_IP = request->hasParam("ssid_staticip", true);
    if(config->SSID_STATIC_IP)
    {
      config->SSID_IP = (char *)request->getParam("ssid_ip", true)->value().c_str();
      config->SSID_SUBNET = (char *)request->getParam("ssid_subnet", true)->value().c_str();
      config->SSID_GATEWAY = (char *)request->getParam("ssid_gateway", true)->value().c_str();
      config->SSID_PRIMARY_DNS = (char *)request->getParam("primary_dns", true)->value().c_str();
      config->SSID_SECONDARY_DNS = (char *)request->getParam("secondary_dns", true)->value().c_str();
    }
  }

  config->MQTT_SERVER = (char *)request->getParam("mqtt_server", true)->value().c_str();
  config->MQTT_USERNAME = (char *)request->getParam("mqtt_username", true)->value().c_str();
  config->MQTT_PASSWORD = (char *)request->getParam("mqtt_password", true)->value().c_str();
  config->MQTT_TOPIC_NAME = (char *)request->getParam("mqtt_topic_name", true)->value().c_str();
  config->MQTT_USE_JSONTABLE = request->hasParam("mqtt_jsontable", true);
  config->MQTT_USE_ONETOPIC = request->hasParam("mqtt_use_onetopic", true);

  if(config->MQTT_USE_ONETOPIC)
  {
    config->MQTT_ONETOPIC_NAME = (char *)request->getParam("mqtt_onetopic_name", true)->value().c_str();
  }

  config->MQTT_PORT = request->getParam("mqtt_port", true)->value().toInt();
  config->FREQUENCY = request->getParam("frequency", true)->value().toInt();
  config->PIN_ENABLE_CONFIG = request->getParam("pin_enable_config", true)->value().toInt();

  config->X10A_ENABLED = request->hasParam("x10a_enabled", true);
  config->HEATING_ENABLED = request->hasParam("heating_enabled", true);
  config->COOLING_ENABLED = request->hasParam("cooling_enabled", true);
  config->SG_ENABLED = request->hasParam("sg_enabled", true);
  config->CAN_ENABLED = request->hasParam("can_enabled", true);
  config->CAN_CONFIG = CANConfig;

  if(config->X10A_ENABLED)
  {
    config->PIN_RX = request->getParam("pin_rx", true)->value().toInt();
    config->PIN_TX = request->getParam("pin_tx", true)->value().toInt();
    config->X10A_PROTOCOL = (X10AProtocol)request->getParam("x10a_protocol", true)->value().toInt();
  }

  if(config->HEATING_ENABLED)
    config->PIN_HEATING = request->getParam("pin_heating", true)->value().toInt();

  if(config->COOLING_ENABLED)
    config->PIN_COOLING = request->getParam("pin_cooling", true)->value().toInt();

  if(config->SG_ENABLED)
  {
    config->PIN_SG1 = request->getParam("pin_sg1", true)->value().toInt();
    config->PIN_SG2 = request->getParam("pin_sg2", true)->value().toInt();
    config->SG_RELAY_HIGH_TRIGGER = request->hasParam("sg_relay_trigger", true);
  }

  if(request->hasParam("definedParameters", true))
  {
    DynamicJsonDocument jsonParameters(MODELS_CONFIG_SIZE);
    deserializeJson(jsonParameters, request->getParam("definedParameters", true)->value());
    JsonArray parametersArray = jsonParameters.as<JsonArray>();

    config->PARAMETERS_LENGTH = parametersArray.size();
    config->PARAMETERS = new ParameterDef*[config->PARAMETERS_LENGTH];

    int counter = 0;
    for (JsonArray value : parametersArray)
    {
      config->PARAMETERS[counter] = new ParameterDef(
        value[0].as<const int>(),
        value[1].as<const int>(),
        value[2].as<const int>(),
        value[3].as<const int>(),
        value[4].as<const int>(),
        value[5]);
      counter++;
    }
  }
  else
  {
    config->PARAMETERS_LENGTH = 0;
    config->PARAMETERS = nullptr;
  }

  StaticJsonDocument<WEBUI_SELECTION_VALUE_SIZE> webuiSelectionValues;
  webuiSelectionValues["model"] = (char *)request->getParam("model", true)->value().c_str();
  webuiSelectionValues["language"] = (char *)request->getParam("language", true)->value().c_str();
  webuiSelectionValues["presetParameters"] = (char *)request->getParam("presetParameters", true)->value().c_str();

  String serializedWebuiSelectionValues;
  serializeJson(webuiSelectionValues, serializedWebuiSelectionValues);
  config->WEBUI_SELECTION_VALUES = (char *)serializedWebuiSelectionValues.c_str();

  saveConfig();

  request->onDisconnect([]()
  {
    restart_board();
  });

  debugSerial.println("Saving successfull. Restarting...");
  request->send(200, "text/plain", "OK");
}

void onUpdate(AsyncWebServerRequest *request)
{
  bool hasError = Update.hasError();

  request->onDisconnect([hasError]()
  {
    if(hasError || !updateVersionValid)
      return;

    restart_board();
  });

  AsyncWebServerResponse *response = request->beginResponse((hasError)?500:200, "text/plain", (hasError)?"FAIL":((updateVersionValid)?"OK":"INVALID"));
  response->addHeader("Connection", "close");
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

void handleUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  //Upload handler chunks in data
  if (!index)
  {
    updateVersionValid = false;

    debugSerial.print("Start Web OTA Update - MD5: ");

    if(!request->hasParam("MD5", true))
    {
      return request->send(400, "text/plain", "MD5 parameter missing");
    }

    const char* md5 = request->getParam("MD5", true)->value().c_str();

    if(strlen(md5) != 32)
    {
      return request->send(400, "text/plain", "MD5 parameter invalid");
    }

    debugSerial.println(md5);

    #if defined(ESP8266)
        int cmd = (filename == "filesystem") ? U_FS : U_FLASH;
        Update.runAsync(true);
        size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin((cmd == U_FS)?fsSize:maxSketchSpace, cmd)){ // Start with max available size
    #elif defined(ESP32)
        int cmd = (request->getParam("type", true)->value() == "filesystem") ? U_SPIFFS : U_FLASH;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) { // Start with max available size
    #endif
        Update.printError(Serial);
        return request->send(400, "text/plain", "OTA could not begin");
    }
    Update.setMD5(md5);
    webOTAIsBusy = true;
  }

  // Write chunked data to the free sketch space
  if(len)
  {
    if (Update.write(data, len) != len)
    {
        return request->send(400, "text/plain", "OTA could not begin");
    }
    debugSerial.print(".");
  }

  if (final)
  { // if the final flag is set then this is the last frame of data
    const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t* runningPartition = esp_ota_get_running_partition();
    debugSerial.print("\n--> Update finished!\n");
    webOTAIsBusy = false;
    if (!Update.end(true))
    { //true to set the size to the current progress
      Update.printError(debugSerial);
      return request->send(400, "text/plain", "Could not end OTA");
    }

    esp_app_desc_t appRunning, appUpdate;
    esp_ota_get_partition_description(runningPartition, &appRunning);
    esp_ota_get_partition_description(updatePartition, &appUpdate);

    SemanticVersion runningVersion(appRunning.version);
    SemanticVersion updateVersion(appUpdate.version);

        // if current or update version are dev-version (no version nr)
    if (!runningVersion.hasVersionNr ||
        !updateVersion.hasVersionNr ||
        // or if new version is higher than old one
         runningVersion.Major < updateVersion.Major ||
        (runningVersion.Major <= updateVersion.Major &&
         runningVersion.Minor < updateVersion.Minor) ||
        (runningVersion.Major <= updateVersion.Major &&
         runningVersion.Minor <= updateVersion.Minor &&
         runningVersion.Patch < updateVersion.Patch) ||
        (runningVersion.Major <= updateVersion.Major &&
         runningVersion.Minor <= updateVersion.Minor &&
         runningVersion.Patch <= updateVersion.Patch &&
         runningVersion.Build <  updateVersion.Build) ||
        // or if new version is equal
        (runningVersion.Major == updateVersion.Major &&
         runningVersion.Minor == updateVersion.Minor &&
         runningVersion.Patch == updateVersion.Patch &&
         runningVersion.Build == updateVersion.Build)) {
      // --> accept update
      updateVersionValid = true;
    }
  }
}

void onUploadCANFile(AsyncWebServerRequest *request)
{
  if(!request->hasParam("file", true, true))
  {
    request->send(422, "text/plain", "Missing CAN file");
    return;
  }

  String fsFilename = lastUploadFileName;
  debugSerial.printf("Found LittleFS Filename: %s\n", fsFilename.c_str());

  File canCommandsFile = LittleFS.open(CAN_COMMANDS_FILE, FILE_READ);
  DynamicJsonDocument canCommandsDoc(MODELS_DOC_SIZE);
  DeserializationError result;
  if((result = deserializeJson(canCommandsDoc, canCommandsFile)).code() != DeserializationError::Code::Ok) {
    debugSerial.printf("CAN Commands file read error: %i\n", result.code());
    return;
  };
  JsonArray canCommandsDocArr = canCommandsDoc.as<JsonArray>();
  canCommandsFile.close();

  File uploadFileFS = LittleFS.open(fsFilename, FILE_READ);
  DynamicJsonDocument uploadDoc(COMMANDS_DEFINITION_UPLOAD_SIZE);
  if((result = deserializeJson(uploadDoc, uploadFileFS)).code() != DeserializationError::Code::Ok) {
    debugSerial.printf("CAN definition file read error: %i\n", result.code());
    return;
  };
  uploadFileFS.close();

  bool newModel = true;
  for (JsonObject canCommands : canCommandsDocArr)
  {
    if(strcmp(canCommands["Model"].as<const char*>(), uploadDoc["Model"].as<const char*>()) == 0)
    {
      debugSerial.printf("Found existing Model: %s\n", canCommands["Model"].as<const char*>());

      newModel = false;

      bool existingLanguage = false;
      for (JsonPair kv : canCommands["Files"].as<JsonObject>())
      {
        if(strcmp(kv.key().c_str(), uploadDoc["Language"].as<const char*>()) == 0)
        {
          debugSerial.printf("Found existing Model file: %s\n", kv.key().c_str());
          fsFilename = kv.value().as<const char*>();
          existingLanguage = true;
          break;
        }
      }

      if(!existingLanguage)
      {
        debugSerial.printf("add new language to existing Model file: %s\n", uploadDoc["Language"].as<const char*>());
        canCommands["Files"][uploadDoc["Language"].as<const char *>()] = fsFilename;
      }
    }
  }

  if(newModel)
  {
    debugSerial.printf("Found new Model: %s\n", uploadDoc["Model"].as<const char*>());

    JsonObject newModelObect = canCommandsDocArr.createNestedObject();
    newModelObect["Model"] = uploadDoc["Model"].as<const char*>();
    newModelObect["Files"][uploadDoc["Language"].as<const char *>()] = fsFilename;
  }

  serializeJson(canCommandsDoc, Serial);

  canCommandsFile = LittleFS.open(CAN_COMMANDS_FILE, FILE_WRITE);
  serializeJson(canCommandsDoc, canCommandsFile);
  canCommandsFile.close();

  request->send(200);
}

void onWebSerialCallback(const uint8_t *data, const size_t len)
{
  WebSerial.println(F("Received Data..."));

  String inputMessage = "";
  for(int i=0; i < len; i++){
    inputMessage += char(data[i]);
  }

  WebSerial.println(inputMessage);

  if(inputMessage == "freeMemory")
  {
    WebSerial.printf("Heap size: %d bytes\n", ESP.getHeapSize());
    WebSerial.printf("Free memory: %d bytes\n", esp_get_free_heap_size());
    WebSerial.printf("Free memory Heap: %d bytes\n", ESP.getFreeHeap());
    WebSerial.printf("Lowest avaiable Memory: %d bytes\n", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    WebSerial.printf("LittleFS space used: %d/%d bytes\n", LittleFS.usedBytes(), LittleFS.totalBytes());
  }
  else if(inputMessage == "getFragmentation")
  {
    WebSerial.print(F("Fragmentation is: "));
    WebSerial.println(100 - heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) * 100.0 / heap_caps_get_free_size(MALLOC_CAP_8BIT));
  }
}

void onReset(AsyncWebServerRequest *request)
{
  request->onDisconnect([]()
  {
    restart_board();
  });

  request->send(200, "text/javascript", "OK");
}

void onLoadCANValuesResult(AsyncWebServerRequest *request)
{
  if(valueCANLoadState == NotLoading)
  {
    request->send(503, "text/plain", "No values loading in progress");
    return;
  }

  if(valueCANLoadState == Loading || valueCANLoadState == Pending)
  {
    request->send(503, "text/plain", "Values loading not finished");
    return;
  }

  if(valueCANLoadResponse == "") {
    request->send(404, "application/json", "CAN-Bus initialization with given parameter failed. Check if CAN config is correct.");
    valueCANLoadResponse = "";
  } else {
    request->send(200, "application/json", valueCANLoadResponse);
    valueCANLoadResponse = "";
  }

  valueCANLoadState = NotLoading;

  delete webuiScanCANRegisterConfig;
}

void onLoadCANValues(AsyncWebServerRequest *request)
{
  if(valueCANLoadState != NotLoading) {
    request->send(202, "text/plain", "Value loading in progress");
    return;
  }

  if(!handleCAN(request, &webuiScanCANRegisterConfig)) {
    return;
  }

  valueCANLoadState = Pending;

  request->send(200, "application/json", "OK");
}

void WebUI_Init()
{
  if(!LittleFS.exists(MODELS_FILE))
  {
    formatDefaultFS();
  }

  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  WebSerial.onMessage(onWebSerialCallback);

  server.on("/", HTTP_GET, onIndex);
  server.on("/pico.min.css", HTTP_GET, onRequestPicoCSS);
  server.on("/main.css", HTTP_GET, onRequestMainCSS);
  server.on("/main.js", HTTP_GET, onRequestMainJS);
  server.on("/md5.min.js", HTTP_GET, onRequestMD5JS);
  server.on("/loadModel", HTTP_POST, onLoadModel);
  server.on("/loadCommand", HTTP_POST, onLoadCommand);
  server.on("/loadBoardInfo", HTTP_GET, onLoadBoardInfo);
  server.on("/loadModels", HTTP_GET, onLoadModels);
  server.on("/loadCommands", HTTP_GET, onLoadCommands);
  server.on("/loadValues", HTTP_POST, onLoadValues);
  server.on("/loadValuesResult", HTTP_GET, onLoadValuesResult);
  server.on("/saveConfig", HTTP_POST, onSaveConfig);
  server.on("/exportConfig", HTTP_GET, onExportConfig);
  server.on("/loadConfig", HTTP_GET, onLoadConfig);
  server.on("/can/loadValues", HTTP_POST, onLoadCANValues);
  server.on("/can/loadValuesResult", HTTP_GET, onLoadCANValuesResult);
  server.on("/wifi/loadNetworks", HTTP_GET, onWifiLoadNetworks);
  server.on("/wifi/loadFinished", HTTP_GET, onWifiLoadFinished);
  server.on("/format", HTTP_GET, onFormat);
  server.on("/reset", HTTP_GET, onReset);
  server.on("/update", HTTP_POST, onUpdate, handleUpdate);
  server.on("/upload/config", HTTP_POST, onUploadConfigFile, handleFileUpload);
  server.on("/upload/X10A", HTTP_POST, onUploadX10AFile, handleFileUpload);
  server.on("/upload/CAN", HTTP_POST, onUploadCANFile, handleFileUpload);
  server.begin();
}