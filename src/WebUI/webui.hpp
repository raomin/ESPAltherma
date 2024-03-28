#ifndef WEBUI_H
#define WEBUI_H

#include <LittleFS.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <Updater.h>
#else
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#endif

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

#if not __has_include("../webui/webfileHeaders.h")
#error Please run the PlatformIO prep-build script before compiling!
#else
#include "../webui/webfileHeaders.h"
#endif

#define MODELS_FILE "/models.json"
#define CAN_COMMANDS_FILE "/CANCommands.json"
#define MODEL_DEFINITION_DOC_SIZE 1024*25
#define MODEL_DEFINITION_UPLOAD_SIZE 1024*50
#define COMMANDS_DEFINITION_UPLOAD_SIZE 1024*70
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

void WebUI_Init();

#endif