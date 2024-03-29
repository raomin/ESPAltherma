#ifndef WEBUI_H
#define WEBUI_H

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <ArduinoJson.h>
#include "X10A/comm.hpp"
#include "Config/config.hpp"
#include "wireless.hpp"
#include "persistence.hpp"
#include "arrayFunctions.hpp"
#include "webuiBackgroundTasks.hpp"
#include "semanticVersion.hpp"
#include "Config/boardDefaults.hpp"

#ifdef ARDUINO_ARCH_ESP8266
#include <Updater.h>

#define LittleFS_open(file, mode) LittleFS.open(file, mode)
#define HEAP_FRAGMENTATION() ESP.getHeapFragmentation()
#else
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>

#define LittleFS_open(file, mode) LittleFS.open(file, mode, true)
#define HEAP_FRAGMENTATION() (100 - heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) * 100.0 / heap_caps_get_free_size(MALLOC_CAP_8BIT))
#endif

#if not __has_include("../build/webui/webfileHeaders.h")
#define index_html_gz ""
#define index_html_gz_len 0
#define pico_min_css_gz ""
#define pico_min_css_gz_len 0
#define main_css_gz ""
#define main_css_gz_len 0
#define main_js_gz ""
#define main_js_gz_len 0
#define md5_min_js_gz ""
#define md5_min_js_gz_len 0

#warning Missing generated webfiles header
#else
#include "../build/webui/webfileHeaders.h"
#endif

#define MODELS_FILE "/models.json"
#define CAN_COMMANDS_FILE "/CANCommands.json"
#define MODEL_DEFINITION_DOC_SIZE 1024*25
#define MODEL_DEFINITION_UPLOAD_SIZE 1024*50
#define COMMANDS_DEFINITION_UPLOAD_SIZE 1024*70
#define MODELS_DOC_SIZE 1024*10
#define WEBUI_SELECTION_VALUE_SIZE 1024

extern String lastUploadFileName;
extern bool webOTAIsBusy;

enum class LoopRunStatus : uint8_t {
    Running,
    Stopping,
    Stopped
};

extern LoopRunStatus mainLoopStatus;

void WebUI_Init();

#endif