#include <string>
#include "labeldef.h"

/*
 * Creates the full Device Discovery payload, see https://www.home-assistant.io/integrations/mqtt/#device-discovery-payload.
 */
std::string buildDeviceDiscoveryPayload(const LabelDef *labels, size_t count);

/*
 * Turns a LabelDef into a "component" JSON element for inclusion in the "cmp" array of the Device Discovery payload.
 */
std::string makeSensorJson(const LabelDef &label, bool isDiagnostic);

/*
 * Returns sensor properties "p"(latform), "dev_cla" (device_class) and "unit_of_meas"(urement).
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
