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

char jsonbuffhttps[MAX_MSG_SIZE] = "{\0";


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
      #ifdef ARDUINO_M5Stick_C_Plus2
      M5.Lcd.print("[HTTPS] POST...\n");
      #endif
      https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // start connection and send HTTP header
    int httpCode = https.POST(httpRequestData);
    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
      #ifdef ARDUINO_M5Stick_C_Plus2
      M5.Lcd.printf("[HTTPS] POST... code: %d\n", httpCode);
      #endif
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        // print server response payload
        String payload = https.getString();
        Serial.println(payload);
        #ifdef ARDUINO_M5Stick_C_Plus2
        M5.Lcd.println(payload);
        #endif
    }

  }
  else {
   Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
   #ifdef ARDUINO_M5Stick_C_Plus2
   M5.Lcd.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
   #endif
  }

  https.end();
  }
}


void sendValuesHTTPS()
{
  Serial.printf("Sending values via HTTPS.\n");
#ifdef ARDUINO_M5Stick_C_Plus2
  //Add Power values
  // getBatteryVoltage returns battery voltage [mV] as an int16_t
  float batteryVoltage = (float) M5.Power.getBatteryVoltage() / 1000; // convert to V as a float
  snprintf(jsonbuffhttps + strlen(jsonbuffhttps),MAX_MSG_SIZE - strlen(jsonbuffhttps) , "\"%s\":\"%.3gV\",", "M5BatV", batteryVoltage);
#elif ARDUINO_M5Stick_C
  //Add M5 APX values
  snprintf(jsonbuffhttps + strlen(jsonbuffhttps),MAX_MSG_SIZE - strlen(jsonbuffhttps) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5VIN", M5.Axp.GetVinVoltage(),"M5AmpIn", M5.Axp.GetVinCurrent());
  snprintf(jsonbuffhttps + strlen(jsonbuffhttps),MAX_MSG_SIZE - strlen(jsonbuffhttps) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5BatV", M5.Axp.GetBatVoltage(),"M5BatCur", M5.Axp.GetBatCurrent());
  snprintf(jsonbuffhttps + strlen(jsonbuffhttps),MAX_MSG_SIZE - strlen(jsonbuffhttps) , "\"%s\":\"%.3gmW\",", "M5BatPwr", M5.Axp.GetBatPower());
#endif
  snprintf(jsonbuffhttps + strlen(jsonbuffhttps),MAX_MSG_SIZE - strlen(jsonbuffhttps) , "\"%s\":\"%ddBm\",", "WifiRSSI", WiFi.RSSI());
  snprintf(jsonbuffhttps + strlen(jsonbuffhttps),MAX_MSG_SIZE - strlen(jsonbuffhttps) , "\"%s\":\"%d\",", "FreeMem", ESP.getFreeHeap());
  jsonbuffhttps[strlen(jsonbuffhttps) - 1] = '}';

  posthttps(jsonbuffhttps);
  #ifdef ARDUINO_M5Stick_C_Plus2
  M5.Lcd.println(jsonbuffhttps); //Optional - prints the data to the screen if required
  #endif

  // wipes the jsonbuffhttps back to {null
  strcpy(jsonbuffhttps, "{\0");

}




