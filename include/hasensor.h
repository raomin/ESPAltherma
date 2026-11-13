#ifndef ESPALTHERMA_HASENSOR_H
#define ESPALTHERMA_HASENSOR_H

#include "labeldef.h"

// Discovery only needs metadata. Extra sensors have no registry bytes or converted-value buffer.
struct HaSensor
{
  const char *label;
  int dataType, convid;

  constexpr HaSensor(const char *label = nullptr, int dataType = -1, int convid = -1)
      : label(label), dataType(dataType), convid(convid) {}

  LabelDef definition() const { return LabelDef(-1, -1, convid, -1, dataType, label); }
};

#endif
