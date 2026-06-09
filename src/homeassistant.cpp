#include <string>
#include <cstring>
#include <cctype>
#include <HardwareSerial.h>
#include <homeassistant.h>

const char *discovery_json_start = "{\"dev\":{\"ids\":\"espaltherma-mqtt-discovery\",\"name\":\"Daikin Altherma via ESPAltherma\",\"mf\":\"Daikin\",\"mdl\":\"Altherma\"},\"o\":{\"name\":\"espaltherma-mqtt-discovery\",\"sw\":\"0.1\",\"url\":\"https://github.com/raomin/ESPAltherma/\"},\"cmps\":{";
const char *discovery_json_end = "},\"stat_t\":\"espaltherma/ATTR\",\"qos\": 2}";

#define ESP_SENSOR_M5BATV 999
#define ESP_SENSOR_WIFI_RSSI 998
#define ESP_SENSOR_FREE_MEM 997

const LabelDef battery_voltage = {-1, -1, -1, -1, ESP_SENSOR_M5BATV, "M5BatV"};
const LabelDef wifi_rssi = {-1, -1, -1, -1, ESP_SENSOR_WIFI_RSSI, "WifiRSSI"};
const LabelDef free_mem = {-1, -1, -1, -1, ESP_SENSOR_FREE_MEM, "FreeMem"};

size_t streamDeviceDiscoveryPayload(const LabelDef *labels, size_t count, DiscoverySink sink, void *ctx)
{
    size_t total = 0;
    auto emit = [&](const char *data, size_t len) {
        total += len;
        if (sink != nullptr)
        {
            sink(ctx, data, len);
        }
    };

    emit(discovery_json_start, strlen(discovery_json_start));

    // Hardcoded ESP diagnostic sensors.
    std::string s = makeSensorJson(battery_voltage, true);
    emit(s.data(), s.size());
    s = "," + makeSensorJson(wifi_rssi, true);
    emit(s.data(), s.size());
    s = "," + makeSensorJson(free_mem, true);
    emit(s.data(), s.size());

    // Sensors from the active definition file. Each component is streamed on its own so the
    // payload is never held in memory all at once.
    for (size_t i = 0; i < count; i++)
    {
        s = "," + makeSensorJson(labels[i], false);
        emit(s.data(), s.size());
    }

    emit(discovery_json_end, strlen(discovery_json_end));

    return total;
}

std::string makeSensorJson(const LabelDef &label, bool isDiagnostic)
{
    const std::string key = makeJsonKey(label.label);
    const std::string uid = "espaltherma_" + key;

    std::string json;
    json.reserve(512);

    json += "\"" + key + "\":{";
    json += getSensorDeviceAndUnit(label);
    json += "\"val_tpl\":\"{{value_json['";
    json += label.label;
    json += "']";
    json += getConversion(label);
    json += "}}\",";
    json += "\"uniq_id\":\"" + uid + "\",";
    json += "\"def_ent_id\":\"sensor." + uid + "\",";
    json += "\"name\":\"";
    json += label.label;
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
            return "|replace('V','')|float";
        case ESP_SENSOR_WIFI_RSSI:
            return "|replace('dBm','')|int";
        case ESP_SENSOR_FREE_MEM:
            return "|int";
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
            return "\"p\":\"sensor\",\"dev_cla\":\"voltage\",\"unit_of_meas\":\"V\",";
        // "WifiRSSI" (signal_strength in dBm)
        case ESP_SENSOR_WIFI_RSSI:
            return "\"p\":\"sensor\",\"dev_cla\":\"signal_strength\",\"unit_of_meas\":\"dBm\",";
        // "FreeMem" (data_size in b)
        case ESP_SENSOR_FREE_MEM:
            return "\"p\":\"sensor\",\"dev_cla\":\"data_size\",\"unit_of_meas\":\"B\",";

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
        return "\"p\":\"binary_sensor\",";
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

    // Fan speed (rotations per second).
    if (label.convid == 152 && strstr(label.label, "(rps)") != NULL)
    {
        return "\"p\":\"sensor\",\"dev_cla\":\"frequency\",\"unit_of_meas\":\"Hz\",";
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