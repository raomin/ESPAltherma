#ifndef ESPALTHERMA_BOARD_H
#define ESPALTHERMA_BOARD_H

// Board profile: name reported in diagnostics/telemetry and default X10A pins
// used when the configuration does not set them.

#if !defined(ARDUINO_ARCH_ESP32)
#error "ESPAltherma 2 runs on the ESP32 family. For the ESP8266, use ESPAltherma 1.x (main branch)."
#endif

#if defined(ARDUINO_M5Stick_C_Plus2)
#define BOARD_NAME "m5stickcplus2"
#elif defined(ARDUINO_M5Stick_C_Plus)
#define BOARD_NAME "m5stickcplus"
#elif defined(ARDUINO_M5Stick_C)
#define BOARD_NAME "m5stickc"
#elif defined(ARDUINO_M5Stack_Tough)
#define BOARD_NAME "m5stack-tough"
#elif defined(ARDUINO_WT32_ETH01)
#define BOARD_NAME "wt32-eth01"
#elif defined(ARDUINO_ESP32_POE) || defined(ARDUINO_ESP32_POE_ISO)
#define BOARD_NAME "esp32-poe"
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
#elif defined(ARDUINO_WT32_ETH01)
#define BOARD_DEFAULT_RX_PIN 5 // RXD/TXD pins of the header
#define BOARD_DEFAULT_TX_PIN 17
#elif defined(ARDUINO_ESP32_POE) || defined(ARDUINO_ESP32_POE_ISO)
#define BOARD_DEFAULT_RX_PIN 36 // UEXT connector (GPIO17 is the Ethernet clock)
#define BOARD_DEFAULT_TX_PIN 4
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define BOARD_DEFAULT_RX_PIN 4 // GPIO16/17 do not exist on the C3
#define BOARD_DEFAULT_TX_PIN 5
#else
#define BOARD_DEFAULT_RX_PIN 16 // Default GPIO PINs for Serial2
#define BOARD_DEFAULT_TX_PIN 17
#endif

// The C3/S3 web flasher builds use the chip's USB port as Serial (ARDUINO_USB_CDC_ON_BOOT). Boards with a
// USB-UART bridge talk on UART0 instead: the log and Improv use both, so one firmware fits both kinds.
#if ARDUINO_USB_CDC_ON_BOOT && (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3))
#define SECOND_CONSOLE Serial0
#endif

// Wired Ethernet: build with -D HAS_ETHERNET. The PHY settings (ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_POWER,
// ETH_PHY_MDC, ETH_PHY_MDIO, ETH_CLK_MODE) come from the board variant (WT32-ETH01, Olimex ESP32-PoE) or from
// build flags, eg. for an IP101 PHY: -D ETH_PHY_TYPE=ETH_PHY_IP101 -D ETH_PHY_ADDR=1 -D ETH_PHY_POWER=5
#if defined(HAS_ETHERNET) && (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3))
#error "HAS_ETHERNET: wired Ethernet (RMII PHY) needs a classic ESP32"
#endif

#endif
