#ifndef ESPALTHERMA_DEFROSTEXPORT_H
#define ESPALTHERMA_DEFROSTEXPORT_H

#include <stdio.h>
#include "earlydefrost.h"

inline int defrostJsonRow(const DefrostSample &s,bool first,char *out,size_t size)
{
  return snprintf(out,size,"%s[%u,%d,%d,%u,%u]",first?"":",\n",s.deltaSeconds,s.outdoorHalf,s.coilHalf,s.rps,s.flags);
}
inline int defrostCsvRow(const DefrostSample &s,uint32_t seconds,char *out,size_t size)
{
  char ambient[12]="",coil[12]="",rps[8]="";
  if (s.outdoorHalf!=INT8_MIN) snprintf(ambient,sizeof(ambient),"%.1f",s.outdoorHalf/2.0);
  if (s.coilHalf!=INT8_MIN) snprintf(coil,sizeof(coil),"%.1f",s.coilHalf/2.0);
  if (s.rps!=255) snprintf(rps,sizeof(rps),"%u",s.rps);
  return snprintf(out,size,"%lu,%s,%s,%s,%s,%s,%s,%s\n",(unsigned long)seconds,ambient,coil,rps,
    s.flags&DF_STATUS?(s.flags&DF_STARTUP?"1":"0"):"",s.flags&DF_STATUS?(s.flags&DF_DEFROST?"1":"0"):"",
    s.flags&DF_DHW_KNOWN?(s.flags&DF_DHW?"1":"0"):"",s.flags&DF_STATUS?(s.flags&DF_COOLING?"1":"0"):"");
}

#endif
