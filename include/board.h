#ifndef ESPALTHERMA_BOARD_H
#define ESPALTHERMA_BOARD_H

// Board profile: name reported in diagnostics/telemetry and default X10A pins
// used when the configuration does not set them (generic web-flasher builds).

#if defined(ARDUINO_M5Stick_C_Plus2)
#define BOARD_NAME "m5stickcplus2"
#elif defined(ARDUINO_M5Stick_C_Plus)
#define BOARD_NAME "m5stickcplus"
#elif defined(ARDUINO_M5Stick_C)
#define BOARD_NAME "m5stickc"
#elif defined(ARDUINO_M5Stack_Tough)
#define BOARD_NAME "m5stack-tough"
#elif defined(ARDUINO_ARCH_ESP8266)
#define BOARD_NAME "esp8266"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define BOARD_NAME "esp32c3"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define BOARD_NAME "esp32s3"
#else
#define BOARD_NAME "esp32"
#endif

#if defined(ARDUINO_M5Stick_C_Plus2) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
#define BOARD_DEFAULT_RX_PIN 36 // Pin connected to the TX pin of X10A
#define BOARD_DEFAULT_TX_PIN 26 // Pin connected to the RX pin of X10A
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define BOARD_DEFAULT_RX_PIN 4 // GPIO16/17 do not exist on the C3
#define BOARD_DEFAULT_TX_PIN 5
#else
#define BOARD_DEFAULT_RX_PIN 16 // Default GPIO PINs for Serial2
#define BOARD_DEFAULT_TX_PIN 17
#endif

// The C3/S3 web flasher builds use the chip's USB port as Serial (ARDUINO_USB_CDC_ON_BOOT). Boards with a
// USB-UART bridge talk on UART0 instead: the log and Improv use both, so one firmware fits both kinds.
#if defined(ARDUINO_ARCH_ESP32) && ARDUINO_USB_CDC_ON_BOOT && (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3))
#define SECOND_CONSOLE Serial0
#endif

// Features that need more than the ESP8266 can offer (NVS, FreeRTOS tasks).
// The ESP8266 keeps the legacy compile-time configuration.
// The label catalog (catalog.h) is const data: in flash on ESP32, but it would take ~18KB of RAM on ESP8266.
#if defined(ARDUINO_ARCH_ESP32)
#define HAS_NVS_CONFIG
#define HAS_HP_TASK
#define HAS_CATALOG
// Web interface, setup access point and Improv provisioning. #define DISABLE_WEBUI in my_setup.h to leave it out.
#if !defined(DISABLE_WEBUI)
#define HAS_WEBUI
#endif
#endif

#endif
