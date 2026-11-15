#ifndef ESPALTHERMA_HASENSOR_H
#define ESPALTHERMA_HASENSOR_H

#include "labeldef.h"

// A sensor the firmware computes itself (statistics, DIII values, the unit's meter and counters), as Home Assistant
// discovery needs it: a name, a type and a conversion. A LabelDef carries registry bytes and a 30-character value
// on top, which these sensors never use: 64 bytes each instead of 12, in RAM for as long as the firmware runs.
struct HaSensor
{
  const char *label;
  int dataType, convid;

  constexpr HaSensor(const char *label = nullptr, int dataType = -1, int convid = -1)
      : label(label), dataType(dataType), convid(convid) {}

  LabelDef definition() const { return LabelDef(-1, -1, convid, -1, dataType, label); }
};

#endif
