#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <Preferences.h>
#include "Config/config.hpp"
#include "debugSerial.hpp"

#define NAME_NAMESPACE "ESPAltherma"
#define NAME_INITED "Inited"
#define NAME_STATE_HEATING "Heating"
#define NAME_STATE_COOLING "Cooling"
#define NAME_STATE_SAFETY "Safety"

extern Preferences preferences;

void resetPersistence();

void initPersistence();

void savePersistence();

void readPersistence();

#endif