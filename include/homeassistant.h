#include <string>
#include "labeldef.h"

/*
 * Sink callback used to stream the Device Discovery payload one chunk at a time.
 */
typedef void (*DiscoverySink)(void *ctx, const char *data, size_t len);

/*
 * Streams the full Device Discovery payload (see
 * https://www.home-assistant.io/integrations/mqtt/#device-discovery-payload) through `sink`,
 * one chunk at a time, and returns the total payload length in bytes.
 * Pass sink == nullptr to only compute the length (needed up-front for MQTT's beginPublish()).
 * The payload is never materialized as one large buffer, keeping heap usage low on the ESP.
 */
size_t streamDeviceDiscoveryPayload(const LabelDef *labels, size_t count, DiscoverySink sink, void *ctx);

/*
 * Turns a LabelDef into a "component" JSON element for inclusion in the "cmp" array of the Device Discovery payload.
 */
std::string makeSensorJson(const LabelDef &label, bool isDiagnostic);

/*
 * Returns sensor properties "p"(latform), "dev_cla" (device_class) and "unit_of_meas"(urement).
 * Numeric sensors receive a measurement state class when the component JSON is assembled.
 */
std::string getSensorDeviceAndUnit(const LabelDef &label);

/*
 * Returns an optional conversion for the given label.
 */
std::string getConversion(const LabelDef &label);

/*
 * Turns a label name into a JSON-safe key by lowercasing it and replacing spaces with underscores. Uniqueness not guaranteed.
 */
std::string makeJsonKey(const char *input);
