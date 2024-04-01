#ifndef SETUP_H
#define SETUP_H

//Setup your credentials and mqtt info here:
//only change the value between the " " leave the rest of the line untouched.
#define WIFI_SSID ""//**Your SSID here**
#define WIFI_PWD ""//**Your password here** leave empty if open (bad!)

//Uncomment this to set a static IP instead of DHCP for the ESP (Separate by commas instead of dots)
//#define WIFI_IP 192, 168, 0, 5
//#define WIFI_SUBNET 255, 255, 255, 0
//#define WIFI_GATEWAY 192, 168, 0, 1
//#define WIFI_PRIMARY_DNS 8, 8, 8, 8     //A DNS address is needed, even if it's not used 
//#define WIFI_SECONDARY_DNS 8, 8, 4, 4   //A DNS address is needed, even if it's not used

//#define MQTT //We want to send MQTT messages to a MQTT Server (comment out if we don't)

#define MQTT_SERVER "192.168.x.x"//**IP address here of your MQTT server**
#define MQTT_USERNAME ""//leave empty if not set (bad!)
#define MQTT_PASSWORD ""//leave empty if not set (bad!)
#define MQTT_PORT 1883
//#define MQTT_ENCRYPTED // uncomment if MQTT connection is encrypted via TLS

#define FREQUENCY 30000 //query values every 30 sec

// #define ARDUINO_M5Stick_C_Plus2 //phil added - might not be necessary as covered by platformio.ini file

#if defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C_Plus2)
// Values used when M5StickC, M5STickCPlus or M5Stick_C_Plus2 environment is selected:
#define RX_PIN    36// Pin connected to the TX pin of X10A 
#define TX_PIN    26// Pin connected to the RX pin of X10A
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
//uncomment and set to enable the safety relay
//#define SAFETY_RELAY_PIN 33// Pin connected to the safety relay. Triggered HIGH


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
#include "def/Altherma(EBLA-EDLA D series 9-16kW Monobloc).h"
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

// HTTPS FUNCTIONS -------------------
// Uncomment the below 'define HTTPS' to send HTTPS Messages
#define HTTPS
#if defined (HTTPS)
#define HTTP_SERVER "https://emoncms.org/input/post"  //emoncms
#define APIKEY "" //emoncms
#define NODE "emontx" // node in emoncms

// root certificate of emoncms.org - expires 2038
#define rootCACertificate \
"-----BEGIN CERTIFICATE-----\n" \
"MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB\n" \
"iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl\n" \
"cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV\n" \
"BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw\n" \
"MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV\n" \
"BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n" \
"aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy\n" \
"dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK\n" \
"AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B\n" \
"3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY\n" \
"tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/\n" \
"Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2\n" \
"VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT\n" \
"79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6\n" \
"c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT\n" \
"Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l\n" \
"c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee\n" \
"UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE\n" \
"Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd\n" \
"BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G\n" \
"A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF\n" \
"Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO\n" \
"VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3\n" \
"ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs\n" \
"8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR\n" \
"iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze\n" \
"Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ\n" \
"XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/\n" \
"qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB\n" \
"VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB\n" \
"L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG\n" \
"jjxDah2nGN59PRbxYvnKkKj9\n" \
"-----END CERTIFICATE-----"
#endif

#endif //SETUP_H