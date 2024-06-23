#include "config.hpp"

ESPAlthermaConfig* config = nullptr;

ESPAlthermaConfig::~ESPAlthermaConfig()
{
    if(X10A_ENABLED) {
        delete X10A_CONFIG;
    }

    if(CAN_ENABLED) {
        delete CAN_CONFIG;
    }
}

void readConfig()
{
    if(config != nullptr)
        delete config;

    config = new ESPAlthermaConfig();

    if(!LittleFS.exists(CONFIG_FILE))
        return;

    File configFile = LittleFS.open(CONFIG_FILE, FILE_READ);
    size_t configFileSize = configFile.size();
    DynamicJsonDocument configDoc(MODELS_CONFIG_SIZE);
    deserializeJson(configDoc, configFile);
    configFile.close();

    serializeJsonPretty(configDoc, Serial);
    Serial.println();

    if(configFileSize == 0)
        return;

    config->configStored = true;
    config->STANDALONE_WIFI = configDoc["STANDALONE_WIFI"].as<const bool>();

    if(!config->STANDALONE_WIFI) {
        config->SSID = (char *)configDoc["SSID"].as<const char*>();
        config->SSID_PASSWORD = (char *)configDoc["SSID_PASSWORD"].as<const char*>();
        config->SSID_STATIC_IP = configDoc["SSID_STATIC_IP"].as<const bool>();
        if(config->SSID_STATIC_IP) {
            config->SSID_IP = (char *)configDoc["SSID_IP"].as<const char*>();
            config->SSID_SUBNET = (char *)configDoc["SSID_SUBNET"].as<const char*>();
            config->SSID_GATEWAY = (char *)configDoc["SSID_GATEWAY"].as<const char*>();
            config->SSID_PRIMARY_DNS = (char *)configDoc["SSID_PRIMARY_DNS"].as<const char*>();
            config->SSID_SECONDARY_DNS = (char *)configDoc["SSID_SECONDARY_DNS"].as<const char*>();
        }
    }

    config->MQTT_SERVER = (char *)configDoc["MQTT_SERVER"].as<const char*>();
    config->MQTT_USERNAME = (char *)configDoc["MQTT_USERNAME"].as<const char*>();
    config->MQTT_PASSWORD = (char *)configDoc["MQTT_PASSWORD"].as<const char*>();
    config->MQTT_TOPIC_NAME = (char *)configDoc["MQTT_TOPIC_NAME"].as<const char*>();
    config->MQTT_USE_JSONTABLE = configDoc["MQTT_USE_JSONTABLE"].as<const bool>();
    config->MQTT_USE_ONETOPIC = configDoc["MQTT_USE_ONETOPIC"].as<const bool>();

    if(config->MQTT_USE_ONETOPIC) {
        config->MQTT_ONETOPIC_NAME = (char *)configDoc["MQTT_ONETOPIC_NAME"].as<const char*>();
    }

    config->MQTT_PORT = configDoc["MQTT_PORT"].as<uint16_t>();
    config->MQTT_DISABLE_LOG_MESSAGES = configDoc["MQTT_DISABLE_LOG_MESSAGES"].as<const bool>();
    config->MQTT_ENCRYPTED = configDoc["MQTT_ENCRYPTED"].as<const bool>();
    config->PIN_ENABLE_CONFIG = configDoc["PIN_ENABLE_CONFIG"].as<uint8_t>();
    config->HEATING_ENABLED = configDoc["HEATING_ENABLED"].as<const bool>();
    config->PIN_HEATING = configDoc["PIN_HEATING"].as<uint8_t>();
    config->COOLING_ENABLED = configDoc["COOLING_ENABLED"].as<const bool>();
    config->PIN_COOLING = configDoc["PIN_COOLING"].as<uint8_t>();
    config->SG_ENABLED = configDoc["SG_ENABLED"].as<const bool>();
    config->PIN_SG1 = configDoc["PIN_SG1"].as<uint8_t>();
    config->PIN_SG2 = configDoc["PIN_SG2"].as<uint8_t>();
    config->SG_RELAY_HIGH_TRIGGER = configDoc["SG_RELAY_HIGH_TRIGGER"].as<const bool>();
    config->X10A_ENABLED = configDoc["X10A_ENABLED"].as<const bool>();
    config->CAN_ENABLED = configDoc["CAN_ENABLED"].as<const bool>();
    config->SAFETY_ENABLED = configDoc["SAFETY_ENABLED"].as<const bool>();
    config->PIN_SAFETY = configDoc["PIN_SAFETY"].as<uint8_t>();

    if(config->X10A_ENABLED) {
        X10A_Config* X10AConfig = new X10A_Config();
        JsonObject configX10ADoc = configDoc["X10A_CONFIG"];
        X10AConfig->FREQUENCY = configX10ADoc["FREQUENCY"].as<uint32_t>();
        X10AConfig->PIN_RX = configX10ADoc["PIN_RX"].as<uint8_t>();
        X10AConfig->PIN_TX = configX10ADoc["PIN_TX"].as<uint8_t>();
        X10AConfig->X10A_PROTOCOL = (X10AProtocol)configX10ADoc["X10A_PROTOCOL"].as<uint8_t>();
        X10AConfig->WEBUI_SELECTION_VALUES = (char *)configX10ADoc["WEBUI_SELECTION_VALUES"].as<const char*>();

        x10a_fill_config(configX10ADoc, X10AConfig);

        config->X10A_CONFIG = X10AConfig;
    }

    if(config->CAN_ENABLED) {
        CAN_Config* CANConfig = new CAN_Config();
        JsonObject configCANDoc = configDoc["CAN_CONFIG"];
        CANConfig->CAN_IC = (CAN_ICTypes)configCANDoc["CAN_IC"].as<uint8_t>();
        CANConfig->CAN_BUS = (CAN_ICBus)configCANDoc["CAN_BUS"].as<uint8_t>();

        CANConfig->CAN_UART.PIN_RX = configCANDoc["UART"]["PIN_RX"].as<uint8_t>();
        CANConfig->CAN_UART.PIN_TX = configCANDoc["UART"]["PIN_TX"].as<uint8_t>();

        CANConfig->CAN_SPI.PIN_MISO = configCANDoc["SPI"]["MISO"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_MOSI = configCANDoc["SPI"]["MOSI"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_SCK = configCANDoc["SPI"]["SCK"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_CS = configCANDoc["SPI"]["CS"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_INT = configCANDoc["SPI"]["INT"].as<uint8_t>();
        CANConfig->CAN_SPI.IC_MHZ = configCANDoc["SPI"]["MHZ"].as<uint8_t>();

        CANConfig->CAN_BLUETOOTH.DEVICENAME = (char *)configCANDoc["BLUETOOTH"]["DEVICENAME"].as<const char*>();
        CANConfig->CAN_BLUETOOTH.USE_PIN = configCANDoc["BLUETOOTH"]["USE_PIN"].as<const bool>();
        CANConfig->CAN_BLUETOOTH.PIN = (char *)configCANDoc["BLUETOOTH"]["PIN"].as<const char*>();

        CANConfig->CAN_SPEED_KBPS = configCANDoc["CAN_SPEED_KBPS"].as<uint16_t>();
        CANConfig->CAN_MQTT_TOPIC_NAME = (char *)configCANDoc["CAN_MQTT_TOPIC_NAME"].as<const char*>();
        CANConfig->CAN_READONLY_ENABLED = configCANDoc["CAN_READONLY_ENABLED"].as<const bool>();
        CANConfig->CAN_SNIFFING_ENABLED = configCANDoc["CAN_SNIFFING_ENABLED"].as<const bool>();
        CANConfig->CAN_AUTOPOLL_MODE = (CAN_PollMode)configCANDoc["CAN_AUTOPOLL_MODE"].as<uint8_t>();
        if(CANConfig->CAN_AUTOPOLL_MODE == CAN_PollMode::Auto) {
            CANConfig->CAN_AUTOPOLL_TIME = configCANDoc["CAN_AUTOPOLL_TIME"].as<uint16_t>();
        }

        canBus_fill_config(configCANDoc, CANConfig);

        config->CAN_CONFIG = CANConfig;
    }
}

void saveConfig()
{
    DynamicJsonDocument configDoc(MODELS_CONFIG_SIZE);
    configDoc["STANDALONE_WIFI"] = config->STANDALONE_WIFI;

    if(!config->STANDALONE_WIFI) {
        configDoc["SSID"] = config->SSID;
        configDoc["SSID_PASSWORD"] = config->SSID_PASSWORD;
        configDoc["SSID_STATIC_IP"] = config->SSID_STATIC_IP;

        if(config->SSID_STATIC_IP) {
            configDoc["SSID_IP"] = config->SSID_IP;
            configDoc["SSID_SUBNET"] = config->SSID_SUBNET;
            configDoc["SSID_GATEWAY"] = config->SSID_GATEWAY;
            configDoc["SSID_PRIMARY_DNS"] = config->SSID_PRIMARY_DNS;
            configDoc["SSID_SECONDARY_DNS"] = config->SSID_SECONDARY_DNS;
        }
    }

    configDoc["MQTT_SERVER"] = config->MQTT_SERVER;
    configDoc["MQTT_USERNAME"] = config->MQTT_USERNAME;
    configDoc["MQTT_PASSWORD"] = config->MQTT_PASSWORD;
    configDoc["MQTT_TOPIC_NAME"] = config->MQTT_TOPIC_NAME;
    configDoc["MQTT_USE_JSONTABLE"] = config->MQTT_USE_JSONTABLE;
    configDoc["MQTT_USE_ONETOPIC"] = config->MQTT_USE_ONETOPIC;

    if(config->MQTT_USE_ONETOPIC) {
        configDoc["MQTT_ONETOPIC_NAME"] = config->MQTT_ONETOPIC_NAME;
    }

    configDoc["MQTT_PORT"] = config->MQTT_PORT;
    configDoc["MQTT_DISABLE_LOG_MESSAGES"] = config->MQTT_DISABLE_LOG_MESSAGES;
    configDoc["MQTT_ENCRYPTED"] = config->MQTT_ENCRYPTED;
    configDoc["PIN_ENABLE_CONFIG"] = config->PIN_ENABLE_CONFIG;
    configDoc["HEATING_ENABLED"] = config->HEATING_ENABLED;
    configDoc["PIN_HEATING"] = config->PIN_HEATING;
    configDoc["COOLING_ENABLED"] = config->COOLING_ENABLED;
    configDoc["PIN_COOLING"] = config->PIN_COOLING;
    configDoc["SG_ENABLED"] = config->SG_ENABLED;
    configDoc["PIN_SG1"] = config->PIN_SG1;
    configDoc["PIN_SG2"] = config->PIN_SG2;
    configDoc["SG_RELAY_HIGH_TRIGGER"] = config->SG_RELAY_HIGH_TRIGGER;
    configDoc["X10A_ENABLED"] = config->X10A_ENABLED;
    configDoc["CAN_ENABLED"] = config->CAN_ENABLED;
    configDoc["SAFETY_ENABLED"] = config->SAFETY_ENABLED;
    configDoc["PIN_SAFETY"] = config->PIN_SAFETY;

    if(config->X10A_ENABLED) {
        JsonObject X10AConfig = configDoc.createNestedObject("X10A_CONFIG");
        X10AConfig["FREQUENCY"] = config->X10A_CONFIG->FREQUENCY;
        X10AConfig["PIN_RX"] = config->X10A_CONFIG->PIN_RX;
        X10AConfig["PIN_TX"] = config->X10A_CONFIG->PIN_TX;
        X10AConfig["X10A_PROTOCOL"] = (uint8_t)config->X10A_CONFIG->X10A_PROTOCOL;
        X10AConfig["WEBUI_SELECTION_VALUES"] = config->X10A_CONFIG->WEBUI_SELECTION_VALUES;

        JsonArray parameters = X10AConfig.createNestedArray("PARAMETERS");
        for(size_t i = 0; i < config->X10A_CONFIG->PARAMETERS_LENGTH; i++) {
            JsonArray parameter = parameters.createNestedArray();
            parameter.add(config->X10A_CONFIG->PARAMETERS[i]->registryID);
            parameter.add(config->X10A_CONFIG->PARAMETERS[i]->offset);
            parameter.add(config->X10A_CONFIG->PARAMETERS[i]->convid);
            parameter.add(config->X10A_CONFIG->PARAMETERS[i]->dataSize);
            parameter.add(config->X10A_CONFIG->PARAMETERS[i]->dataType);
            parameter.add(config->X10A_CONFIG->PARAMETERS[i]->label);
        }
    }

    if(config->CAN_ENABLED) {
        JsonObject CANConfig = configDoc.createNestedObject("CAN_CONFIG");
        CANConfig["CAN_IC"] = (uint8_t)config->CAN_CONFIG->CAN_IC;
        CANConfig["CAN_BUS"] = (uint8_t)config->CAN_CONFIG->CAN_BUS;

        JsonObject canUART = CANConfig.createNestedObject("UART");
        canUART["PIN_RX"] = config->CAN_CONFIG->CAN_UART.PIN_RX;
        canUART["PIN_TX"] = config->CAN_CONFIG->CAN_UART.PIN_TX;

        JsonObject canSPI = CANConfig.createNestedObject("SPI");
        canSPI["MISO"] = config->CAN_CONFIG->CAN_SPI.PIN_MISO;
        canSPI["MOSI"] = config->CAN_CONFIG->CAN_SPI.PIN_MOSI;
        canSPI["SCK"] = config->CAN_CONFIG->CAN_SPI.PIN_SCK;
        canSPI["CS"] = config->CAN_CONFIG->CAN_SPI.PIN_CS;
        canSPI["INT"] = config->CAN_CONFIG->CAN_SPI.PIN_INT;
        canSPI["MHZ"] = config->CAN_CONFIG->CAN_SPI.IC_MHZ;

        JsonObject canBT = CANConfig.createNestedObject("BLUETOOTH");
        canBT["DEVICENAME"] = config->CAN_CONFIG->CAN_BLUETOOTH.DEVICENAME;
        canBT["USE_PIN"] = config->CAN_CONFIG->CAN_BLUETOOTH.USE_PIN;
        canBT["PIN"] = config->CAN_CONFIG->CAN_BLUETOOTH.PIN;

        CANConfig["CAN_SPEED_KBPS"] = config->CAN_CONFIG->CAN_SPEED_KBPS;
        CANConfig["CAN_MQTT_TOPIC_NAME"] = config->CAN_CONFIG->CAN_MQTT_TOPIC_NAME;
        CANConfig["CAN_READONLY_ENABLED"] = config->CAN_CONFIG->CAN_READONLY_ENABLED;
        CANConfig["CAN_SNIFFING_ENABLED"] = config->CAN_CONFIG->CAN_SNIFFING_ENABLED;
        CANConfig["CAN_AUTOPOLL_MODE"] = (uint8_t)config->CAN_CONFIG->CAN_AUTOPOLL_MODE;
        if(config->CAN_CONFIG->CAN_AUTOPOLL_MODE == CAN_PollMode::Auto) {
            CANConfig["CAN_AUTOPOLL_TIME"] = config->CAN_CONFIG->CAN_AUTOPOLL_TIME;
        }

        JsonArray commands = CANConfig.createNestedArray("COMMANDS");
        for(size_t i = 0; i < config->CAN_CONFIG->COMMANDS_LENGTH; i++) {
            JsonArray command = commands.createNestedArray();
            command.add(config->CAN_CONFIG->COMMANDS[i]->name);
            command.add(config->CAN_CONFIG->COMMANDS[i]->label);

            JsonArray commandBytes = command.createNestedArray();
            for (uint8_t j = 0; j < CAN_COMMAND_BYTE_LENGTH; j++) {
                commandBytes.add(config->CAN_CONFIG->COMMANDS[i]->command[j]);
            }

            command.add(config->CAN_CONFIG->COMMANDS[i]->id);
            command.add(config->CAN_CONFIG->COMMANDS[i]->divisor);
            command.add(config->CAN_CONFIG->COMMANDS[i]->writable);
            command.add(config->CAN_CONFIG->COMMANDS[i]->unit);
            command.add(config->CAN_CONFIG->COMMANDS[i]->type);

            if(config->CAN_CONFIG->COMMANDS[i]->valueCodeSize > 0) {
                JsonObject valueCodeObject = command.createNestedObject();

                for(uint8_t j = 0; j < config->CAN_CONFIG->COMMANDS[i]->valueCodeSize; j++) {
                    valueCodeObject[config->CAN_CONFIG->COMMANDS[i]->valueCode[j]->key] = config->CAN_CONFIG->COMMANDS[i]->valueCode[j]->value;
                }
            }
        }
    }

    File configFile = LittleFS.open(CONFIG_FILE, FILE_WRITE);
    serializeJsonPretty(configDoc, Serial);
    serializeJson(configDoc, configFile);
    configFile.close();
}