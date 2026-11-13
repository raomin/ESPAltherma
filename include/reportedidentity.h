#ifndef ESPALTHERMA_REPORTEDIDENTITY_H
#define ESPALTHERMA_REPORTEDIDENTITY_H

#include "survey.h"

// Format on demand from the last survey; no permanent duplicate. An EEPROM part number or MPU ID
// does not identify an outdoor model or an exact firmware image.
struct ReportedIdentity
{
  char hydroSoftware[9] = {}, outdoorPart[16] = {}, outdoorMpu[5] = {};
  const char *hydroSource = nullptr;
  int outdoorRevision = -1;

  void read(const Survey &s)
  {
    *this = ReportedIdentity();
    if (s.protocol != 'I') return;
    bool zero = true, ff = true;
    for (size_t i = 0; i < 4; i++) { zero = zero && s.swId[i] == 0; ff = ff && s.swId[i] == 0xFF; }
    if (s.hasSwId && !zero && !ff)
    {
      snprintf(hydroSoftware, sizeof(hydroSoftware), "%02X%02X%02X%02X", s.swId[3], s.swId[2], s.swId[1], s.swId[0]);
      hydroSource = "L";
    }
    else
    {
      int lo = s.byteAt(0x60, 14), hi = s.byteAt(0x60, 15);
      if (lo >= 0 && hi >= 0 && !(lo == 0 && hi == 0) && !(lo == 255 && hi == 255))
      {
        snprintf(hydroSoftware, sizeof(hydroSoftware), "%02X%02X", hi, lo);
        hydroSource = "0x60";
      }
    }
    if (surveyPNumber(s, outdoorPart, sizeof(outdoorPart)))
    {
      int revision = s.byteAt(0x11, 5);
      if (revision >= 0 && revision != 255) outdoorRevision = revision;
    }
    int a = s.byteAt(0x00, 10), b = s.byteAt(0x00, 11);
    if (a >= 0 && b >= 0 && !(a == 0 && b == 0) && !(a == 255 && b == 255))
      snprintf(outdoorMpu, sizeof(outdoorMpu), "%02X%02X", a, b);
  }
};

#endif
