#ifndef WEBUI_H
#define WEBUI_H

#include <LittleFS.h>
#include <Update.h>
#include "ESPAsyncWebServer.h"
#include <WebSerialLite.h>
#include <esp_ota_ops.h>
#include "esp_task_wdt.h"
#include "ArduinoJson.h"
#include "X10A/comm.hpp"
#include "Config/config.hpp"
#include "wireless.hpp"
#include "persistence.hpp"
#include "arrayFunctions.hpp"
#include "webuiBackgroundTasks.hpp"
#include "SemanticVersion.hpp"

#define MODELS_FILE "/models.json"
#define CAN_COMMANDS_FILE "/CANCommands.json"
#define MODEL_DEFINITION_DOC_SIZE 1024*25
#define MODEL_DEFINITION_UPLOAD_SIZE 1024*50
#define COMMANDS_DEFINITION_UPLOAD_SIZE 1024*100
#define MODELS_DOC_SIZE 1024*10
#define WEBUI_SELECTION_VALUE_SIZE 1024

extern const uint8_t mainJS_start[] asm("_binary_main_js_gz_start");
extern const uint8_t mainJS_end[] asm("_binary_main_js_gz_end");
extern const uint8_t md5JS_start[] asm("_binary_md5_min_js_gz_start");
extern const uint8_t md5JS_end[] asm("_binary_md5_min_js_gz_end");
extern const uint8_t indexHTML_start[] asm("_binary_index_html_gz_start");
extern const uint8_t indexHTML_end[] asm("_binary_index_html_gz_end");
extern const uint8_t picoCSS_start[] asm("_binary_pico_min_css_gz_start");
extern const uint8_t picoCSS_end[] asm("_binary_pico_min_css_gz_end");
extern const uint8_t mainCSS_start[] asm("_binary_main_css_gz_start");
extern const uint8_t mainCSS_end[] asm("_binary_main_css_gz_end");

extern String lastUploadFileName;
extern bool webOTAIsBusy;

enum class LoopRunStatus : uint8_t {
    Running,
    Stopping,
    Stopped
};

extern LoopRunStatus mainLoopStatus;

bool formatDefaultFS();

void onWifiLoadNetworks(AsyncWebServerRequest *request);

void onWifiLoadFinished(AsyncWebServerRequest *request);

void onLoadBoardInfo(AsyncWebServerRequest *request);

void onIndex(AsyncWebServerRequest *request);

void onRequestPicoCSS(AsyncWebServerRequest *request);

void onRequestMainCSS(AsyncWebServerRequest *request);

void onRequestMainJS(AsyncWebServerRequest *request);

void onRequestMD5JS(AsyncWebServerRequest *request);

void onFormat(AsyncWebServerRequest *request);

void onLoadModels(AsyncWebServerRequest *request);

void onLoadCommands(AsyncWebServerRequest *request);

void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t* data, size_t len, bool final);

void onUploadX10AFile(AsyncWebServerRequest *request);

void onUploadConfigFile(AsyncWebServerRequest *request);

void onLoadValuesResult(AsyncWebServerRequest *request);

void onLoadValues(AsyncWebServerRequest *request);

void onLoadModel(AsyncWebServerRequest *request);

void onLoadCommand(AsyncWebServerRequest *request);

void onLoadConfig(AsyncWebServerRequest *request);

void onExportConfig(AsyncWebServerRequest *request);

void onSaveConfig(AsyncWebServerRequest *request);

void onUpdate(AsyncWebServerRequest *request);

void handleUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

void onUploadCANFile(AsyncWebServerRequest *request);

void WebUI_Init();

#endif