#include "labeldef.h"
//  This file is a definition file for ESPAltherma: Daikin Altherma HT, the high temperature hydrobox EKHBRD011-016
//  (R134a second stage), read with the older protocol S on the same X41A as "Altherma(HT EKHBRD-ERRQ series 11-16kW)".
//  The board answers both protocols; this one carries both pressures and the discharge and liquid temperatures of
//  the stage, which protocol I does not. Written for issue #95 from two owners' readings on EKHBRD016ACV1 units.
//  uncomment each value you want to query for your installation.
//
//  Auto-detection keeps protocol I when both answer: choose this model from the list to read these values.
//  Not answered by the hydrobox here: the indoor air, outdoor air and outdoor exchanger temperatures of 0x54.


LabelDef labelDefs[] = {
//{0x00,0,804,0,-1,"*Refrigerant type"},
// 0x50: the two pressures of the stage, kgf/cm2 gauge. The high pressure collapses to 2-3 during defrost.
//{0x50,0,103,2,2,"High Pressure (stage)"},
//{0x50,0,403,2,1,"High Pressure(T)"},
//{0x50,2,103,2,2,"Low Pressure (stage)"},
//{0x50,2,403,2,1,"Low Pressure(T)"},
// 0x53: the actuators. EV pulses big-endian, parked at 500 when stopped; 128 Hz in hot water mode, 60-80 in heating
//{0x53,0,152,2,-1,"EV (pls)"},
//{0x53,4,151,1,-1,"INV Comp. Frequency(Hz)"},
//{0x53,5,200,1,-1,"Comp. Preheat"},
//{0x53,7,200,1,-1,"4 Way Valve"},
//{0x53,10,200,1,-1,"Crankcase Heater"},
// 0x54: temperatures of the stage. The liquid temperature follows the water being heated.
//{0x54,8,109,2,1,"Discharge pipe temp."},
//{0x54,10,103,2,1,"Fin Temp."},
//{0x54,14,103,2,1,"R134a liquid temp."},
// 0x55: not checked on the hydrobox yet
//{0x55,0,201,1,-1,"Operation Mode"},
//{0x55,1,204,1,-1,"Error Code"},
//{0x55,2,204,1,-1,"Thermo Off Error"},
//{0x55,3,204,1,-1,"Warning Code"},
//{0x55,4,204,1,-1,"Caution Code"},
};

// Override protocol
#define PROTOCOL 'S'
