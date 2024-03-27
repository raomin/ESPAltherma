#include <HttpClient.h>
#include <WiFiClientSecure.h>
#include "setup.h"
#ifdef ARDUINO_M5Stick_C_Plus2
#include <M5StickCPlus2.h>
#elif ARDUINO_M5Stick_C_Plus
#include <M5StickCPlus.h>
#elif ARDUINO_M5Stick_C
#include <M5StickC.h>
#endif

void posthttps(char* httpsbuff) 
{
String httpRequestData = "node=" + String(NODE) + "&data=" + String(httpsbuff) + "&apikey=" + APIKEY;  //Build the Emoncms data string

static WiFiClientSecure client;
  if (!client) {
    client.setCACert(rootCACertificate);
  }

    // setup https
  HTTPClient https; 

  if (https.begin(client, HTTP_SERVER)) {
      Serial.print("[HTTPS] POST...\n");
      M5.Lcd.print("[HTTPS] POST...\n");
      https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // start connection and send HTTP header
    int httpCode = https.POST(httpRequestData);
    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
      M5.Lcd.printf("[HTTPS] POST... code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        // print server response payload
        String payload = https.getString();
        Serial.println(payload);
        M5.Lcd.println(payload);
    }

  }
  else {
   Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
   M5.Lcd.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();
  }
}
