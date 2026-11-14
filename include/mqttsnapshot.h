#ifndef ESPALTHERMA_MQTTSNAPSHOT_H
#define ESPALTHERMA_MQTTSNAPSHOT_H
#include <stdint.h>
#include <string.h>
#include <memory>
#include <new>
#include "labeldef.h"

// The strings in LabelDef's metadata belong to the static catalog. Values are mutable, copied separately.
struct HaSnapshotLabel
{
  const char *label, *name;
  int16_t convid, dataType, registryID;
  bool extra;
  LabelDef definition() const {LabelDef d(registryID,-1,convid,-1,dataType,label);d.name=name;return d;}
};

struct HaDiscoverySnapshot
{
  std::unique_ptr<HaSnapshotLabel[]> labels;
  size_t count=0;
  char model[80]={};
  // Caller holds valuesLock throughout capture, never throughout rendering or transmission.
  bool capture(const LabelDef *selected,size_t n,const LabelDef *extras,size_t extraCount,const char *deviceModel)
  {
    if (n>224 || extraCount>224-n || (n && !selected) || (extraCount && !extras)) return false;
    auto next=std::unique_ptr<HaSnapshotLabel[]>(new (std::nothrow) HaSnapshotLabel[n+extraCount]);
    if (!next && n+extraCount) return false;
    for (size_t i=0;i<n+extraCount;i++)
    {
      const LabelDef &d=i<n?selected[i]:extras[i-n];
      next[i]={d.label,d.name,(int16_t)d.convid,(int16_t)d.dataType,(int16_t)d.registryID,i>=n};
    }
    strncpy(model,deviceModel,sizeof(model)-1);model[sizeof(model)-1]=0;
    labels=std::move(next);count=n+extraCount;
    return true;
  }
};
static_assert(sizeof(void*)!=4 || sizeof(HaSnapshotLabel)==16,"compact discovery metadata on ESP32");

struct MqttReading {const char *label;char value[sizeof(LabelDef::asString)];};

// Each publish owns just one copied reading. A label reload ends this pass, avoiding dangling pointers
// and a mixed selection. Lock/unlock encompass only the copy; the sink can block without stopping X10A.
template<typename Result,typename Lock,typename Unlock,typename Sink>
bool publishOneValueSnapshots(LabelDef *&labels,size_t &count,const char *regs,const Result *ok,
                             const uint32_t &generation,Lock lock,Unlock unlock,Sink sink)
{
  lock();uint32_t started=generation;unlock();
  for(size_t i=0;;i++)
  {
    MqttReading reading;bool selected=false;
    lock();
    if (generation!=started || i>=count) {bool done=generation==started;unlock();return done;}
    const LabelDef &d=labels[i];
    for(size_t r=0;r<32 && (uint8_t)regs[r]!=0xFF;r++)
      if ((uint8_t)d.registryID==(uint8_t)regs[r] && ok[r]) {selected=true;break;}
    if (selected)
    {
      reading.label=d.label;memcpy(reading.value,d.asString,sizeof(reading.value));
      reading.value[sizeof(reading.value)-1]=0;
    }
    unlock();
    if (selected) sink(reading);
  }
}
#endif
