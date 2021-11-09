//Setup your credentials and mqtt info here:
//only change the value between the " " leave the rest of the line untouched.
#define WIFI_SSID "lambi"//**Your SSID here**
#define WIFI_PWD "%G1q7U!Ax4O&"//**Your password here** leave empty if open (bad!)

#define MQTT_SERVER "192.168.1.4"//**IP address here of your MQTT server**
#define MQTT_USERNAME ""//leave empty if not set (bad!)
#define MQTT_PASSWORD ""//leave empty if not set (bad!)
#define MQTT_PORT 1883

#define FREQUENCY 30000 //query values every 30 sec

#if defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus)
//Values used when **M5StickC** or **M5STickCPlus** environment is selected:
#define RX_PIN    36// Pin connected to the TX pin of X10A 
#define TX_PIN    26// Pin connected to the RX pin of X10A
#else 
//Default GPIO PINs for Serial2:
#define RX_PIN    16// Pin connected to the TX pin of X10A 
#define TX_PIN    17// Pin connected to the RX pin of X10A
#endif

#define PIN_THERM 0// Pin connected to the thermostat relay (normally open)

//Smart grid control - Optional:
//Uncomment and set to enable SG mqtt functions
//#define PIN_SG1 32// Pin connected to dry contact SG 1 relay (normally open)
//#define PIN_SG2 33// Pin connected to dry contact SG 2 relay (normally open)


#define MAX_MSG_SIZE 4096//max size of the json message sent in mqtt 

//Uncomment this line if the JSON message should be in a Json Table format []. Use only for IOBroker Vis. 
//#define JSONTABLE

//Uncomment this if you want to activate the One Value <-> One Topic mode. Each value will be sent to a specific topic below 
// #define ONEVAL_ONETOPIC
// #define MQTT_OneTopic "espaltherma/OneATTR/" //Keep the ending "/" !!


//Uncomment *ONE* of the following according to your installation.
//Then, open and edit the selected file in the /include/def/ folder and uncomment each values you are interested in.

//To use a locale version eg German, change the path to the file to the localized version. 

//Eg:
//instead of: #include "def/ALTHERMA(HPSU6_ULTRA).h" 
//use: 
//            #include "def/German/ALTHERMA(HPSU6_ULTRA).h"
//
//Suported locales: French, German, Spanish.

//#include "def/ALTHERMA(BIZONE_CB_04-08KW).h"
//#include "def/ALTHERMA(BIZONE_CB_11-16KW).h"
//#include "def/ALTHERMA(GSHP).h"
//#include "def/ALTHERMA(GSHP2).h"
//#include "def/ALTHERMA(HPSU6_ULTRA).h"
//#include "def/ALTHERMA(HYBRID).h"
//#include "def/ALTHERMA(LT-D7_E_BML).h"
//#include "def/ALTHERMA(LT_11-16KW_HYDROSPLIT_HYDRO_UNIT).h"
//#include "def/ALTHERMA(LT_CA_CB_04-08KW).h"
//#include "def/ALTHERMA(LT_CA_CB_11-16KW).h"
//#include "def/ALTHERMA(LT_DA_04-08KW).h"
//#include "def/ALTHERMA(LT_DA_PAIR_BML).h"
//#include "def/ALTHERMA(LT_GAS_INJ)20200702.h"
//#include "def/ALTHERMA(LT_MULTI_DHWHP).h"
//#include "def/ALTHERMA(LT_MULTI_HYBRID).h"
//#include "def/ALTHERMA(MONOBLOC_CA_05-07KW).h"
//#include "def/ALTHERMA(TOP-GRADE).h"
//#include "def/DAIKIN_MINI_INVERTER_CHILLER04-08KW.h"
//#include "def/DEFAULT.h"

#ifndef LABELDEF
#warning "NO DEFINITION SELECTED: Please select your heat pump definition in /src/setup.h -- Using default."
#include "def/DEFAULT.h"
#endif