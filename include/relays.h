#ifndef ESPALTHERMA_RELAYS_H
#define ESPALTHERMA_RELAYS_H

// A reversible unit has two thermostat inputs, one that asks for heating and one that asks for cooling, each closed
// by a relay here. They are never closed together: switching one on releases the other first. True when the other
// one was released, so that the caller opens its relay before closing this one.
inline bool demandSwitch(bool &self, bool &other, bool on)
{
  bool released = on && other;
  if (released)
    other = false;
  self = on;
  return released;
}

#endif
