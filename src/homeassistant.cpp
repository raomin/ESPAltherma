#include <string>
#include <cstring>
#include <cctype>
#include <HardwareSerial.h>
#include <homeassistant.h>

// The device and the topics come from the configurable base topic (topics.h); set by setupMqttClient()
const char *haIdPrefix = "espaltherma";
const char *haDeviceId = "espaltherma-mqtt-discovery";
const char *haDeviceName = "Daikin Altherma via ESPAltherma";
const char *haAttrTopic = "espaltherma/ATTR";

static std::string discoveryStart()
{
    return std::string("{\"dev\":{\"ids\":\"") + haDeviceId + "\",\"name\":\"" + haDeviceName +
           "\",\"mf\":\"Daikin\",\"mdl\":\"Altherma\"},\"o\":{\"name\":\"" + haDeviceId +
           "\",\"sw\":\"0.1\",\"url\":\"https://github.com/raomin/ESPAltherma/\"},\"cmps\":{";
}

static std::string discoveryEnd()
{
    return std::string("},\"stat_t\":\"") + haAttrTopic + "\",\"qos\": 2}";
}

#define ESP_SENSOR_M5BATV 999
#define ESP_SENSOR_WIFI_RSSI 998
#define ESP_SENSOR_FREE_MEM 997
#define ESP_SENSOR_M5VIN 996
#define ESP_SENSOR_UPTIME 995
#define ESP_SENSOR_IP 994

bool haBinaryAsNumber = false;

const LabelDef battery_voltage = {-1, -1, -1, -1, ESP_SENSOR_M5BATV, "M5BatV"};
const LabelDef wifi_rssi = {-1, -1, -1, -1, ESP_SENSOR_WIFI_RSSI, "WifiRSSI"};
const LabelDef free_mem = {-1, -1, -1, -1, ESP_SENSOR_FREE_MEM, "FreeMem"};
const LabelDef m5_vin = {-1, -1, -1, -1, ESP_SENSOR_M5VIN, "M5VIN"};
const LabelDef uptime = {-1, -1, -1, -1, ESP_SENSOR_UPTIME, "Uptime"};
const LabelDef ip_addr = {-1, -1, -1, -1, ESP_SENSOR_IP, "IP"};
const LabelDef *haExtraLabels = nullptr;
size_t haExtraCount = 0;

static bool isMeasurementSensor(const std::string &sensorProperties)
{
    // A numeric converter alone does not make a value a measurement. For example,
    // detailed error codes are numeric but should not participate in statistics.
    return sensorProperties.find("\"p\":\"sensor\"") != std::string::npos &&
           sensorProperties.find("\"unit_of_meas\"") != std::string::npos &&
           sensorProperties.find("\"stat_cla\"") == std::string::npos; // the computed values set their own
}

size_t streamDeviceDiscoveryPayload(const LabelDef *labels, size_t count, DiscoverySink sink, void *ctx,
                                    const std::vector<HaComponent> *removed)
{
    size_t total = 0;
    auto emit = [&](const char *data, size_t len) {
        total += len;
        if (sink != nullptr)
        {
            sink(ctx, data, len);
        }
    };

    std::string s = discoveryStart();
    emit(s.data(), s.size());

    // Hardcoded ESP diagnostic sensors.
    s = makeSensorJson(wifi_rssi, true);
    emit(s.data(), s.size());
#if defined(ARDUINO_M5Stick_C)
    // Battery voltage: only the M5StickC boards (all three build with ARDUINO_M5Stick_C) publish it. Announced on
    // other boards, Home Assistant logs a template warning at every reading.
    s = "," + makeSensorJson(battery_voltage, true);
    emit(s.data(), s.size());
#endif
    s = "," + makeSensorJson(free_mem, true);
    emit(s.data(), s.size());
    s = "," + makeSensorJson(uptime, true);
    emit(s.data(), s.size());
    s = "," + makeSensorJson(ip_addr, true);
    emit(s.data(), s.size());
#if defined(ARDUINO_M5Stick_C) && !defined(ARDUINO_M5Stick_C_Plus2)
    // Supply voltage, published by the AXP192 boards: shows the drops of a weak 5V supply
    s = "," + makeSensorJson(m5_vin, true);
    emit(s.data(), s.size());
#endif

    // Sensors from the active definition file. Each component is streamed on its own so the
    // payload is never held in memory all at once.
    for (size_t i = 0; i < count; i++)
    {
        s = "," + makeSensorJson(labels[i], false);
        emit(s.data(), s.size());
    }
    // The values the firmware computes (energy and cycle statistics)
    for (size_t i = 0; i < haExtraCount; i++)
    {
        s = "," + makeSensorJson(haExtraLabels[i], false);
        emit(s.data(), s.size());
    }
    // Values no longer selected: only their platform, so Home Assistant removes their entities
    for (size_t i = 0; removed != nullptr && i < removed->size(); i++)
    {
        s = haRemovalJson((*removed)[i]);
        emit(s.data(), s.size());
    }

    s = discoveryEnd();
    emit(s.data(), s.size());

    return total;
}

std::vector<HaComponent> haComponents(const LabelDef *labels, size_t count)
{
    std::vector<HaComponent> out;
    for (size_t i = 0; i < count; i++)
    {
        bool binary = getSensorDeviceAndUnit(labels[i]).find("\"p\":\"binary_sensor\"") != std::string::npos;
        out.push_back({makeJsonKey(labels[i].label), binary ? "binary_sensor" : "sensor"});
    }
    for (size_t i = 0; i < haExtraCount; i++)
        out.push_back({makeJsonKey(haExtraLabels[i].label), "sensor"});
    return out;
}

// Translated names may hold quotes
static std::string jsonEscape(const char *s)
{
    std::string out;
    for (; *s; s++)
    {
        if (*s == '"' || *s == '\\')
            out += '\\';
        out += *s;
    }
    return out;
}

std::string makeSensorJson(const LabelDef &label, bool isDiagnostic)
{
    const std::string key = makeJsonKey(label.label);
    const std::string uid = std::string(haIdPrefix) + "_" + key;

    std::string json;
    json.reserve(512);

    const std::string sensorProperties = getSensorDeviceAndUnit(label);
    const bool isBinarySensor = sensorProperties.find("\"p\":\"binary_sensor\"") != std::string::npos;

    json += "\"" + key + "\":{";
    json += sensorProperties;
    if (isMeasurementSensor(sensorProperties))
    {
        json += "\"stat_cla\":\"measurement\",";
    }
    json += "\"val_tpl\":\"{{value_json['";
    json += label.label;
    json += "']";
    json += getConversion(label);
    json += "}}\",";
    json += "\"uniq_id\":\"" + uid + "\",";
    json += "\"def_ent_id\":\"";
    json += isBinarySensor ? "binary_sensor." : "sensor.";
    json += uid + "\",";
    json += "\"name\":\"";
    json += jsonEscape(label.name != nullptr ? label.name : label.label); // in the chosen language; the ids stay English
    json += "\"";

    // "an entity that exposes some configuration parameter or diagnostics of a device but does not allow changing it, for example, a sensor showing RSSI or MAC address"
    if (isDiagnostic)
    {
        json += ",\"ent_cat\":\"diagnostic\"";
    }

    json += "}";

    return json;
}

std::string getConversion(const LabelDef &label)
{
    switch (label.dataType)
    {
        // Backwards-compatible workaround for the built-in ESP sensors returning a string with an optional unit, like "-42dBm" instead of just -42.
        case ESP_SENSOR_M5BATV:
        case ESP_SENSOR_M5VIN:
            return "|replace('V','')|float";
        case ESP_SENSOR_WIFI_RSSI:
            return "|replace('dBm','')|int";
        case ESP_SENSOR_FREE_MEM:
        case ESP_SENSOR_UPTIME:
            return "|int";
        // the computed values are sent as numbers, or null when not known: the template then renders "None", the
        // one word the MQTT sensor takes for unknown (a string such as 'unknown' is refused on a numeric sensor,
        // which keeps its last value)
        case ESP_SENSOR_POWER_KW:
        case ESP_SENSOR_ENERGY_KWH:
        case ESP_SENSOR_RATIO:
        case ESP_SENSOR_COUNT:
        case ESP_SENSOR_MINUTES:
            return std::string(" if value_json['") + label.label + "'] is not none else None";
    }
    return "";
}

std::string getSensorDeviceAndUnit(const LabelDef &label)
{
    switch (label.dataType)
    {
        // Hardcoded ones for ESP:
        // "M5BatV" (voltage in V)
        case ESP_SENSOR_M5BATV:
        case ESP_SENSOR_M5VIN:
            return "\"p\":\"sensor\",\"dev_cla\":\"voltage\",\"unit_of_meas\":\"V\",";
        // "WifiRSSI" (signal_strength in dBm)
        case ESP_SENSOR_WIFI_RSSI:
            return "\"p\":\"sensor\",\"dev_cla\":\"signal_strength\",\"unit_of_meas\":\"dBm\",";
        // "FreeMem" (data_size in b)
        case ESP_SENSOR_FREE_MEM:
            return "\"p\":\"sensor\",\"dev_cla\":\"data_size\",\"unit_of_meas\":\"B\",";
        // "Uptime" (seconds since the last restart)
        case ESP_SENSOR_UPTIME:
            return "\"p\":\"sensor\",\"dev_cla\":\"duration\",\"unit_of_meas\":\"s\",";
        // "IP" (the board's address on the network, a text)
        case ESP_SENSOR_IP:
            return "\"p\":\"sensor\",\"icon\":\"mdi:ip-network\",";
        // the computed statistics (stats.h)
        case ESP_SENSOR_POWER_KW:
            return "\"p\":\"sensor\",\"dev_cla\":\"power\",\"unit_of_meas\":\"kW\",\"sug_dsp_prc\":2,";
        case ESP_SENSOR_ENERGY_KWH: // the day's total: Home Assistant sees the reset at midnight as a new period
            return "\"p\":\"sensor\",\"dev_cla\":\"energy\",\"unit_of_meas\":\"kWh\",\"stat_cla\":\"total_increasing\",\"sug_dsp_prc\":2,";
        case ESP_SENSOR_RATIO:
            return "\"p\":\"sensor\",\"icon\":\"mdi:gauge\",\"stat_cla\":\"measurement\",\"sug_dsp_prc\":2,";
        case ESP_SENSOR_COUNT:
            return "\"p\":\"sensor\",\"icon\":\"mdi:counter\",\"stat_cla\":\"measurement\",";
        case ESP_SENSOR_MINUTES:
            return "\"p\":\"sensor\",\"dev_cla\":\"duration\",\"unit_of_meas\":\"min\",\"stat_cla\":\"measurement\",";

        /// Built-in ones:
        // TODO: move magic numbers to consts
        // temperature
        case 1:
            return "\"p\":\"sensor\",\"dev_cla\":\"temperature\",\"unit_of_meas\":\"°C\",";

        // pressure
        case 2:
            return "\"p\":\"sensor\",\"dev_cla\":\"pressure\",\"unit_of_meas\":\"bar\",";
    }

    // converter magic

    // ON/OFF
    if (label.convid >= 300 && label.convid <= 307 || label.convid == 200)
    {
        return haBinaryAsNumber ? "\"p\":\"binary_sensor\",\"pl_on\":\"1\",\"pl_off\":\"0\"," : "\"p\":\"binary_sensor\",";
    }

    // "Flow sensor (l/min)"
    if (label.convid == 105 && strstr(label.label, "l/min") != NULL)
    {
        return "\"p\":\"sensor\",\"dev_cla\":\"volume_flow_rate\",\"unit_of_meas\":\"L/min\",";
    }

    // Current detection.
    if ((label.convid == 105 && strstr(label.label, "(A)") != NULL) || (label.convid == 161 && strstr(label.label, "Current") != NULL))
    {
        return "\"p\":\"sensor\",\"dev_cla\":\"current\",\"unit_of_meas\":\"A\",";
    }

    // Compressor and fan speeds, in rotations per second (no device class: Home Assistant's "frequency" only takes Hz).
    if (label.convid == 152 && strstr(label.label, "(rps)") != NULL)
    {
        return "\"p\":\"sensor\",\"unit_of_meas\":\"rps\",";
    }

    return "\"p\":\"sensor\",";
}

std::string makeJsonKey(const char *input)
{
    std::string out;
    out.reserve(64);

    for (const char *p = input; *p; ++p)
    {
        unsigned char c = static_cast<unsigned char>(*p);

        if (std::isalnum(c))
        {
            out.push_back(std::tolower(c));
        }
        else if (c == ' ')
        {
            out.push_back('_');
        }
    }
    return out;
}
