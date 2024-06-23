#include "persistence.hpp"

Preferences preferences;

void resetPersistence()
{
  preferences.clear();
}

void initPersistence()
{
  preferences.begin(NAME_NAMESPACE, false);
}

void savePersistence()
{
  if(config->HEATING_ENABLED)
    preferences.putUChar(NAME_STATE_HEATING, digitalRead(config->PIN_HEATING));
  else if(preferences.isKey(NAME_STATE_HEATING))
    preferences.remove(NAME_STATE_HEATING);

  if(config->COOLING_ENABLED)
    preferences.putUChar(NAME_STATE_COOLING, digitalRead(config->PIN_COOLING));
  else if(preferences.isKey(NAME_STATE_COOLING))
    preferences.remove(NAME_STATE_COOLING);

  if(config->SAFETY_ENABLED)
    preferences.putUChar(NAME_STATE_SAFETY, digitalRead(config->PIN_SAFETY));
  else if(preferences.isKey(NAME_STATE_SAFETY))
    preferences.remove(NAME_STATE_SAFETY);
}

void readPersistence()
{
  bool inited = preferences.getBool(NAME_INITED);

  if (inited)
  {
    if(config->HEATING_ENABLED && preferences.isKey(NAME_STATE_HEATING))
    {
      uint8_t heatingState = preferences.getUChar(NAME_STATE_HEATING);
      digitalWrite(config->PIN_HEATING, heatingState);
      debugSerial.printf("Restoring previous heating state: %s\n", (heatingState == HIGH) ? "Off":"On" );
    }

    if(config->COOLING_ENABLED && preferences.isKey(NAME_STATE_COOLING))
    {
      uint8_t coolingState = preferences.getUChar(NAME_STATE_COOLING);
      digitalWrite(config->PIN_COOLING, coolingState);
      debugSerial.printf("Restoring previous cooling state: %s\n", (coolingState == HIGH) ? "Off":"On" );
    }

    if(config->SAFETY_ENABLED && preferences.isKey(NAME_STATE_SAFETY))
    {
      uint8_t safetyState = preferences.getUChar(NAME_STATE_SAFETY);
      digitalWrite(config->PIN_SAFETY, safetyState);
      debugSerial.printf("Restoring previous safety state: %s\n", (safetyState == HIGH) ? "On":"Off" );
    }
  }
  else
  {
    debugSerial.printf("Persistence not initialized (%d). Initializing...\n", inited);
    preferences.putBool(NAME_INITED, true);

    if(config->HEATING_ENABLED)
      digitalWrite(config->PIN_HEATING, HIGH);

    if(config->COOLING_ENABLED)
      digitalWrite(config->PIN_COOLING, HIGH);

    if(config->SAFETY_ENABLED)
      digitalWrite(config->PIN_SAFETY, LOW);

    savePersistence();
  }
}