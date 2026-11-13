#ifndef ESPALTHERMA_DEFROSTLAYOUT_H
#define ESPALTHERMA_DEFROSTLAYOUT_H

#include "earlydefrost.h"
#include "catalog.h"

// Use only the public I-register layout whose complete byte interpretation matches the definition.
// A model correction must still supply these same fields; no guessed RAM/remote-debug addresses.
struct DefrostLayout
{
  uint16_t modeConverter = 0;
  bool configure(int model, const CatalogFix *fixes = nullptr, size_t fixCount = 0)
  {
    modeConverter = 0;
    if (model < 0 || model >= CATALOG_MODEL_COUNT || CATALOG_MODELS[model].protocol != 'I') return false;
    uint8_t fields = 0;
    for (int i=0;i<CATALOG_ENTRY_COUNT;i++)
    {
      const CatalogEntry &e=CATALOG_ENTRIES[i];
      if (!catalogInModel(e,catalogModelFor(e.reg,model,fixes,fixCount))) continue;
      if (e.reg==0x10 && e.offset==0 && (e.conv==201 || e.conv==217) && e.size==1) { modeConverter=e.conv; fields|=1; }
      if (e.reg==0x10 && e.offset==1 && e.conv==304 && e.size==1) fields|=2;
      if (e.reg==0x10 && e.offset==1 && e.conv==305 && e.size==1) fields|=4;
      if (e.reg==0x20 && e.offset==0 && e.conv==105 && e.size==2) fields|=8;
      if (e.reg==0x20 && e.offset==8 && e.conv==105 && e.size==2) fields|=16;
      if (e.reg==0x30 && e.offset==0 && e.conv==152 && e.size==1) fields|=32;
    }
    if (fields != 63) { modeConverter=0; return false; }
    return true;
  }
  void decode(uint8_t reg,const uint8_t *p,size_t length,DefrostInput &in) const
  {
    if (!modeConverter || !p) return;
    if (reg==0x10 && length>=2)
    {
      in.flags |= DF_STATUS;
      if (p[1]&0x20) in.flags |= DF_STARTUP;
      if (p[1]&0x10) in.flags |= DF_DEFROST;
      if (p[0]==2 || (modeConverter==201 && p[0]==5) || (modeConverter==217 && p[0]==6)) in.flags |= DF_COOLING;
    }
    if (reg==0x20 && length>=10)
    {
      in.outdoorTenths=(int16_t)(p[0]|p[1]<<8);
      in.coilTenths=(int16_t)(p[8]|p[9]<<8);
    }
    if (reg==0x30 && length>=1) in.rps=p[0];
  }
};

#endif
