#define MQTT_state "espaltherma/STATE"
#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"
#define MQTT_OnOff "espaltherma/onoff"

char jsonbuff[MAX_MSG_SIZE] = "{\0";
WiFiClient espClient;
PubSubClient client(espClient);

void sendValues()
{
  Serial.printf("Sending values in MQTT.\n");
  jsonbuff[strlen(jsonbuff) - 1] = '}';
  client.publish(MQTT_attr, jsonbuff);
  strcpy(jsonbuff, "{\0");
}


void reconnect()
{
  // Loop until we're reconnected
  int i = 0;
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    if (client.connect("ESPAltherma", MQTT_USERNAME, MQTT_PASSWORD, MQTT_lwt, 0, true, "Offline"))
    {
      Serial.println("connected!");
      client.publish("homeassistant/sensor/espAltherma/config", "{\"name\":\"AlthermaSensors\",\"stat_t\":\"~/STATESENS\",\"avty_t\":\"~/LWT\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\",\"uniq_id\":\"espaltherma\",\"device\":{\"identifiers\":[\"ESPAltherma\"]}, \"~\":\"espaltherma\",\"json_attr_t\":\"~/ATTR\"}", true);
      client.publish(MQTT_lwt, "Online", true);
      client.publish("homeassistant/switch/espAltherma/config", "{\"name\":\"Altherma\",\"cmd_t\":\"~/POWER\",\"stat_t\":\"~/STATE\",\"val_tpl\":\"{{value_json.POWER}}\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);
      // Subscribe
      client.subscribe("espaltherma/POWER");
    }
    else
    {
      Serial.printf("failed, rc=%d, try again in 5 seconds", client.state());
      unsigned long start = millis();
      while (millis() < start + 5000)
      {
        ArduinoOTA.handle();
      }

      if (i++ == 5)
        esp_restart();
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  Serial.printf("Message arrived [%s] : %s\n", topic, payload);
  if (payload[1] == 'F')
  { //Ok I'm not super proud of this, but it works :p turn off
    digitalWrite(PIN_THERM, HIGH);
    client.publish("espaltherma/STATE", "OFF");
    Serial.println("Turned OFF");
  }
  else if (payload[1] == 'N')
  { //turn on
    digitalWrite(PIN_THERM, LOW);
    client.publish("espaltherma/STATE", "ON");
    Serial.println("Turned ON");
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
