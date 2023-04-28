#include "config.hpp"

Config* config = nullptr;

Config::~Config()
{
    if(PARAMETERS_LENGTH) {
        for (size_t i = 0; i < PARAMETERS_LENGTH; i++) {
            delete PARAMETERS[i];
        }
        delete[] PARAMETERS;
    }

    if(CAN_ENABLED) {
        delete CAN_CONFIG;
    }
}

void readConfig()
{
    if(config != nullptr)
        delete config;

    config = new Config();

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
    config->FREQUENCY = configDoc["FREQUENCY"].as<uint32_t>();
    config->PIN_ENABLE_CONFIG = configDoc["PIN_ENABLE_CONFIG"].as<uint8_t>();
    config->X10A_ENABLED = configDoc["X10A_ENABLED"].as<const bool>();
    config->PIN_RX = configDoc["PIN_RX"].as<uint8_t>();
    config->PIN_TX = configDoc["PIN_TX"].as<uint8_t>();
    config->X10A_PROTOCOL = (X10AProtocol)configDoc["X10A_PROTOCOL"].as<uint8_t>();
    config->HEATING_ENABLED = configDoc["HEATING_ENABLED"].as<const bool>();
    config->PIN_HEATING = configDoc["PIN_HEATING"].as<uint8_t>();
    config->COOLING_ENABLED = configDoc["COOLING_ENABLED"].as<const bool>();
    config->PIN_COOLING = configDoc["PIN_COOLING"].as<uint8_t>();
    config->SG_ENABLED = configDoc["SG_ENABLED"].as<const bool>();
    config->PIN_SG1 = configDoc["PIN_SG1"].as<uint8_t>();
    config->PIN_SG2 = configDoc["PIN_SG2"].as<uint8_t>();
    config->SG_RELAY_HIGH_TRIGGER = configDoc["SG_RELAY_HIGH_TRIGGER"].as<const bool>();
    config->CAN_ENABLED = configDoc["CAN_ENABLED"].as<const bool>();

    if(config->CAN_ENABLED) {
        CAN_Config* CANConfig = new CAN_Config();
        CANConfig->CAN_IC = (CAN_ICTypes)configDoc["CAN_IC"].as<uint8_t>();
        CANConfig->CAN_BUS = (CAN_ICBus)configDoc["CAN_BUS"].as<uint8_t>();

        CANConfig->CAN_UART.PIN_RX = configDoc["UART"]["PIN_RX"].as<uint8_t>();
        CANConfig->CAN_UART.PIN_TX = configDoc["UART"]["PIN_TX"].as<uint8_t>();

        CANConfig->CAN_SPI.PIN_MISO = configDoc["SPI"]["MISO"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_MOSI = configDoc["SPI"]["MOSI"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_SCK = configDoc["SPI"]["SCK"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_CS = configDoc["SPI"]["CS"].as<uint8_t>();
        CANConfig->CAN_SPI.PIN_INT = configDoc["SPI"]["INT"].as<uint8_t>();
        CANConfig->CAN_SPI.IC_MHZ = configDoc["SPI"]["MHZ"].as<uint8_t>();

        CANConfig->CAN_BLUETOOTH.DEVICENAME = (char *)configDoc["BLUETOOTH"]["DEVICENAME"].as<const char*>();
        CANConfig->CAN_BLUETOOTH.USE_PIN = configDoc["BLUETOOTH"]["USE_PIN"].as<const bool>();
        CANConfig->CAN_BLUETOOTH.PIN = (char *)configDoc["BLUETOOTH"]["PIN"].as<const char*>();

        CANConfig->CAN_SPEED_KBPS = configDoc["CAN_SPEED_KBPS"].as<uint16_t>();
        CANConfig->CAN_MQTT_TOPIC_NAME = (char *)configDoc["CAN_MQTT_TOPIC_NAME"].as<const char*>();
        CANConfig->CAN_READONLY_ENABLED = configDoc["CAN_READONLY_ENABLED"].as<const bool>();
        CANConfig->CAN_SNIFFING_ENABLED = configDoc["CAN_SNIFFING_ENABLED"].as<const bool>();
        CANConfig->CAN_AUTOPOLL_MODE = (CAN_PollMode)configDoc["CAN_AUTOPOLL_MODE"].as<uint8_t>();
        if(CANConfig->CAN_AUTOPOLL_MODE == CAN_PollMode::Auto) {
            CANConfig->CAN_AUTOPOLL_TIME = configDoc["CAN_AUTOPOLL_TIME"].as<uint16_t>();
        }

        JsonArray commands = configDoc["COMMANDS"].as<JsonArray>();
        CANConfig->COMMANDS_LENGTH = commands.size();
        CANConfig->COMMANDS = new CANCommand*[CANConfig->COMMANDS_LENGTH];

        for(size_t i = 0; i < CANConfig->COMMANDS_LENGTH; i++) {
            JsonArray command = commands[i];

            JsonArray commandBytes = command[CAN_COMMAND_INDEX_COMMAND];
            byte commandArray[] = {
                commandBytes[0],
                commandBytes[1],
                commandBytes[2],
                commandBytes[3],
                commandBytes[4],
                commandBytes[5],
                commandBytes[6]
            };

            CANCommandValueCode** valueCodes;
            uint8_t valueCodeSize = 0;

            if(command.size() > CAN_COMMAND_INDEX_VALUE_CODE) {
                JsonObject valueCodeCommands = command[CAN_COMMAND_INDEX_VALUE_CODE].as<JsonObject>();
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

            CANConfig->COMMANDS[i] = new CANCommand(
                command[CAN_COMMAND_INDEX_NAME],
                command[CAN_COMMAND_INDEX_LABEL],
                commandArray,
                command[CAN_COMMAND_INDEX_ID].as<const uint16_t>(),
                command[CAN_COMMAND_INDEX_DIVISOR].as<const float>(),
                command[CAN_COMMAND_INDEX_WRITABLE].as<const bool>(),
                command[CAN_COMMAND_INDEX_UNIT],
                command[CAN_COMMAND_INDEX_TYPE],
                valueCodeSize,
                valueCodes);
        }

        config->CAN_CONFIG = CANConfig;
    }

    JsonArray parameters = configDoc["PARAMETERS"].as<JsonArray>();
    config->PARAMETERS_LENGTH = parameters.size();
    config->PARAMETERS = new ParameterDef*[config->PARAMETERS_LENGTH];

    for(size_t i = 0; i < config->PARAMETERS_LENGTH; i++) {
        JsonArray parameter = parameters[i];
        config->PARAMETERS[i] = new ParameterDef(
            parameter[0].as<const int>(),
            parameter[1].as<const int>(),
            parameter[2].as<const int>(),
            parameter[3].as<const int>(),
            parameter[4].as<const int>(),
            parameter[5]);
    }

    config->WEBUI_SELECTION_VALUES = (char *)configDoc["WEBUI_SELECTION_VALUES"].as<const char*>();
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
    configDoc["FREQUENCY"] = config->FREQUENCY;
    configDoc["PIN_ENABLE_CONFIG"] = config->PIN_ENABLE_CONFIG;
    configDoc["X10A_ENABLED"] = config->X10A_ENABLED;
    configDoc["PIN_RX"] = config->PIN_RX;
    configDoc["PIN_TX"] = config->PIN_TX;
    configDoc["X10A_PROTOCOL"] = (uint8_t)config->X10A_PROTOCOL;
    configDoc["HEATING_ENABLED"] = config->HEATING_ENABLED;
    configDoc["PIN_HEATING"] = config->PIN_HEATING;
    configDoc["COOLING_ENABLED"] = config->COOLING_ENABLED;
    configDoc["PIN_COOLING"] = config->PIN_COOLING;
    configDoc["SG_ENABLED"] = config->SG_ENABLED;
    configDoc["PIN_SG1"] = config->PIN_SG1;
    configDoc["PIN_SG2"] = config->PIN_SG2;
    configDoc["CAN_ENABLED"] = config->CAN_ENABLED;

    if(config->CAN_ENABLED) {
        configDoc["CAN_IC"] = (uint8_t)config->CAN_CONFIG->CAN_IC;
        configDoc["CAN_BUS"] = (uint8_t)config->CAN_CONFIG->CAN_BUS;

        JsonObject canUART = configDoc.createNestedObject("UART");
        canUART["PIN_RX"] = config->CAN_CONFIG->CAN_UART.PIN_RX;
        canUART["PIN_TX"] = config->CAN_CONFIG->CAN_UART.PIN_TX;

        JsonObject canSPI = configDoc.createNestedObject("SPI");
        canSPI["MISO"] = config->CAN_CONFIG->CAN_SPI.PIN_MISO;
        canSPI["MOSI"] = config->CAN_CONFIG->CAN_SPI.PIN_MOSI;
        canSPI["SCK"] = config->CAN_CONFIG->CAN_SPI.PIN_SCK;
        canSPI["CS"] = config->CAN_CONFIG->CAN_SPI.PIN_CS;
        canSPI["INT"] = config->CAN_CONFIG->CAN_SPI.PIN_INT;
        canSPI["MHZ"] = config->CAN_CONFIG->CAN_SPI.IC_MHZ;

        JsonObject canBT = configDoc.createNestedObject("BLUETOOTH");
        canBT["DEVICENAME"] = config->CAN_CONFIG->CAN_BLUETOOTH.DEVICENAME;
        canBT["USE_PIN"] = config->CAN_CONFIG->CAN_BLUETOOTH.USE_PIN;
        canBT["PIN"] = config->CAN_CONFIG->CAN_BLUETOOTH.PIN;

        configDoc["CAN_SPEED_KBPS"] = config->CAN_CONFIG->CAN_SPEED_KBPS;
        configDoc["CAN_MQTT_TOPIC_NAME"] = config->CAN_CONFIG->CAN_MQTT_TOPIC_NAME;
        configDoc["CAN_READONLY_ENABLED"] = config->CAN_CONFIG->CAN_READONLY_ENABLED;
        configDoc["CAN_SNIFFING_ENABLED"] = config->CAN_CONFIG->CAN_SNIFFING_ENABLED;
        configDoc["CAN_AUTOPOLL_MODE"] = (uint8_t)config->CAN_CONFIG->CAN_AUTOPOLL_MODE;
        if(config->CAN_CONFIG->CAN_AUTOPOLL_MODE == CAN_PollMode::Auto) {
            configDoc["CAN_AUTOPOLL_TIME"] = config->CAN_CONFIG->CAN_AUTOPOLL_TIME;
        }

        JsonArray commands = configDoc.createNestedArray("COMMANDS");
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

    configDoc["SG_RELAY_HIGH_TRIGGER"] = config->SG_RELAY_HIGH_TRIGGER;

    JsonArray parameters = configDoc.createNestedArray("PARAMETERS");
    for(size_t i = 0; i < config->PARAMETERS_LENGTH; i++) {
        JsonArray parameter = parameters.createNestedArray();
        parameter.add(config->PARAMETERS[i]->registryID);
        parameter.add(config->PARAMETERS[i]->offset);
        parameter.add(config->PARAMETERS[i]->convid);
        parameter.add(config->PARAMETERS[i]->dataSize);
        parameter.add(config->PARAMETERS[i]->dataType);
        parameter.add(config->PARAMETERS[i]->label);
    }

    configDoc["WEBUI_SELECTION_VALUES"] = config->WEBUI_SELECTION_VALUES;

    File configFile = LittleFS.open(CONFIG_FILE, FILE_WRITE);
    serializeJsonPretty(configDoc, Serial);
    serializeJson(configDoc, configFile);
    configFile.close();
}