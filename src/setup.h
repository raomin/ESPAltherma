//Setup your credentials and mqtt info here:
//only change the value between the " " leave the rest of the line untouched.
#define WIFI_SSID "SSID"//**Your SSID here**
#define WIFI_PWD "password"//**Your password here** leave empty if open (bad!)

//Uncomment this to set a static IP instead of DHCP for the ESP (Separate by commas instead of dots)
//#define WIFI_IP 192, 168, 0, 5
//#define WIFI_SUBNET 255, 255, 255, 0
//#define WIFI_GATEWAY 192, 168, 0, 1
//#define WIFI_PRIMARY_DNS 8, 8, 8, 8     //A DNS address is needed, even if it's not used 
//#define WIFI_SECONDARY_DNS 8, 8, 4, 4   //A DNS address is needed, even if it's not used

#define MQTT_SERVER "192.168.1.4"//**IP address here of your MQTT server**
#define MQTT_USERNAME ""//leave empty if not set (bad!)
#define MQTT_PASSWORD ""//leave empty if not set (bad!)
#define MQTT_PORT 1883
//#define MQTT_ENCRYPTED // uncomment if MQTT connection is encrypted via TLS

#define FREQUENCY 30000 //query values every 30 sec

#if defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C_Plus2)  || defined(ARDUINO_M5Stack_Tough)
// Values used when M5StickC, M5STickCPlus or M5Stick_C_Plus2 environment is selected:
#define RX_PIN    36// Pin connected to the TX pin of X10A 
#define TX_PIN    26// Pin connected to the RX pin of X10A
#elif defined(ARDUINO_M5Stick_Atom3S_Lite)
// Values used when M5Stick_Atom3S_Lite environment is selected (for groove connector):
#define RX_PIN    32// Pin connected to the TX pin of X10A 
#define TX_PIN    26// Pin connected to the RX pin of X10A
#elif defined(ARDUINO_M5STACK_NANOC6)
// Values used when ARDUINO_M5STACK_NANOC6 environment is selected (for groove connector):
#define RX_PIN    2// Pin connected to the TX pin of X10A 
#define TX_PIN    1// Pin connected to the RX pin of X10A
#else 
//Default GPIO PINs for Serial2:
#define RX_PIN    16// Pin connected to the TX pin of X10A 
#define TX_PIN    17// Pin connected to the RX pin of X10A
#endif

#define PIN_THERM 0// Pin connected to the thermostat relay (normally open)
#define PIN_THERM_ACTIVE_STATE HIGH// State to trigger the thermostat relay

//Smart grid control - Optional:
//Uncomment and set to enable SG mqtt functions
//#define PIN_SG1 32// Pin connected to dry contact SG 1 relay (normally open)
//#define PIN_SG2 33// Pin connected to dry contact SG 2 relay (normally open)
// Define if your SG relay board is Low or High triggered (signal pins)
// Only uncomment one of them
#define SG_RELAY_HIGH_TRIGGER
//#define SG_RELAY_LOW_TRIGGER

// Safety relay - also called Prefered electric fare - Optional
// If the safety relay is triggered, the heat pump will be stopped
// uncomment and set to enable the safety relay
// #define SAFETY_RELAY_PIN 33// Pin connected to the safety relay
// #define SAFETY_RELAY_ACTIVE_STATE HIGH// Pin connected to the safety relay


// DO NOT CHANGE: Defines the SG active/inactive relay states, according to the definition of the trigger status
#if defined(SG_RELAY_LOW_TRIGGER)
#define SG_RELAY_ACTIVE_STATE LOW
#define SG_RELAY_INACTIVE_STATE HIGH
#else
#define SG_RELAY_ACTIVE_STATE HIGH
#define SG_RELAY_INACTIVE_STATE LOW
#endif

#define MAX_MSG_SIZE 7120//max size of the json message sent in mqtt 

//Uncomment this line if the JSON message should be in a Json Table format []. Use only for IOBroker Vis. 
//#define JSONTABLE

//Uncomment this if you want to activate the One Value <-> One Topic mode. Each value will be sent to a specific topic below 
// #define ONEVAL_ONETOPIC
// #define MQTT_OneTopic "espaltherma/OneATTR/" //Keep the ending "/" !!

//Uncomment to disable common frequent log messages via MQTT to reduce network load
//#define DISABLE_LOG_MESSAGES

//Uncomment this line to enable DebugSerial - MQTT<>Serial gateway functionality
//Messages received on espaltherma/serialTX are sent to serial port
//Responses from serial port are published to espaltherma/serialRX
//#define DEBUG_SERIAL


//Uncomment *ONE* of the following according to your installation.
//Then, open and edit the selected file in the /include/def/ folder and uncomment each values you are interested in.

//To use a locale version eg German, change the path to the file to the localized version. 

//Eg:
//instead of: #include "def/Altherma(EGSQH-A series 10kW GEO2).h" 
//use: 
//            #include "def/German/Altherma(EGSQH-A series 10kW GEO2).h"
//
//Suported locales: French, German, Spanish.

//#include "def/DEFAULT.h"

//#include "def/Altherma(EBLA-EDLA D series 4-8kW Monobloc).h"
//#include "def/Altherma(EBLA-EDLA D series 9-16kW Monobloc).h"
//#include "def/Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h"
//#include "def/Altherma(EGSQH-A series 10kW GEO2).h"
//#include "def/Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h"
//#include "def/Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h"
//#include "def/Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h"
//#include "def/Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h"
//#include "def/Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h"
//#include "def/Altherma(EPRA E ETSH-X 16P30-50 E series 8-12kW-ECH2O).h"
//#include "def/Altherma(EPRA E ETV16-ETB16-ETVZ16 E_EJ series 8-12kW).h"
//#include "def/Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h"
//#include "def/Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h"
//#include "def/Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h"
//#include "def/Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h"
//#include "def/Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW).h"
//#include "def/Altherma(ERLA D EBSH-X 16P30-50 D SERIES 11-16kW-ECH2O).h"
//#include "def/Altherma(ERLA D EBV-EBB-EBVZ D SERIES 11-16kW).h"
//#include "def/Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h"
//#include "def/Altherma(Hybrid).h"
//#include "def/Altherma(LT_CA_CB_04-08kW).h"
//#include "def/Altherma(LT_CA_CB_11-16kW).h"
//#include "def/Altherma(LT_CB_04-08kW Bizone).h"
//#include "def/Altherma(LT_CB_11-16kW Bizone).h"
//#include "def/Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h"
//#include "def/Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h"
//#include "def/Altherma(LT_Multi_DHWHP).h"
//#include "def/Altherma(LT_Multi_Hybrid).h"
//#include "def/Daikin Mini chiller(EWAA-EWYA D series 4-8kW).h"
//#include "def/Daikin Mini chiller(EWAA-EWYA D series 9-16kW).h"
//#include "def/Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h"
//#include "def/DEFAULT.h"
//#include "def/EKHWET-BAV3(Multi DHW tank).h"

//#include "def/PROTOCOL_S_ROTEX.h"
//#include "def/PROTOCOL_S.h"


#ifndef LABELDEF
#warning "NO DEFINITION SELECTED: Please select your heat pump definition in /src/setup.h -- Using default."
#include "def/DEFAULT.h"
#endif

// Default to "Protocol I"
#ifndef PROTOCOL
#define PROTOCOL 'I'
#endif
