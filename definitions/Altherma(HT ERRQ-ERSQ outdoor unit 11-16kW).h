#include "labeldef.h"
//  This file is a definition file for ESPAltherma: Daikin Altherma HT (first generation, EKHBRD011-016 hydrobox), read
//  on its OUTDOOR unit ERRQ/ERSQ011-016, the R410A first stage. The outdoor unit has no X10A: it is read on X41A of its
//  main board (A1P), protocol I, five pins like X10A. For the hydrobox's own port, see "Altherma(HT EKHBRD-ERRQ series
//  11-16kW)". Mapped by selman61 on a live ERSQ016AAV1 and checked on an ERRQ016AAV1 by PiotrBeeScale (issue #95).
//
//  The outdoor unit answers 0x00 (10 bytes), 0x10 (16), 0x20 (21), 0x30 (10), 0x40 (the list of the hydrobox
//  addresses) and the hydrobox block 0x50 (19), read with the hydrobox address ("04 40 50 01"): ESPAltherma takes the
//  address from 0x40 and does that by itself. Everything else answers NAK (15 EA). It is recognised by that shape.
//
//  The hydrobox block carries the leaving and return water: on the HT, its two "R134a" temperature fields are the water
//  temperatures, checked against the remote controller within 0.1 C (selman61). This board is the only one that gives
//  the water, the outdoor air and the R410A stage; the hydrobox's port gives the R134a stage.
//  uncomment each value you want to query for your installation.


LabelDef labelDefs[] = {
// Outdoor stage is R410A
//{0x00,0,801,0,-1,"*Refrigerant type"},
//{0x00,0,152,1,-1,"Sensor Data Qty"},
//{0x00,1,152,1,-1,"INV compressor Qty"},
//{0x00,2,152,1,-1,"STD compressor Qty"},
//{0x00,3,152,1,-1,"Fan Data Qty"},
//{0x00,4,152,1,-1,"Expansion Valve Data Qty"},
//{0x00,5,152,1,-1,"4 Way Valve Data Qty"},
//{0x00,6,152,1,-1,"Crank Case Heater Qty"},
//{0x00,7,152,1,-1,"Solenoid valve Qty"},
//{0x00,8,152,1,-1,"Max. connectable indoor units"},
//{0x00,9,152,1,-1,"Connected Indoor Unit Qty"},
//{0x10,0,217,1,-1,"Operation Mode"},
//{0x10,1,307,1,-1,"Thermostat ON/OFF"},
//{0x10,1,306,1,-1,"Restart standby"},
//{0x10,1,304,1,-1,"Defrost Operation"},
//{0x10,1,303,1,-1,"Oil Return Operation"},
//{0x10,1,302,1,-1,"Pressure equalizing operation"},
//{0x10,1,301,1,-1,"Demand Signal"},
//{0x10,1,300,1,-1,"Low noise control"},
//{0x10,4,203,1,-1,"Error type"},
//{0x10,5,204,1,-1,"Error Code"},
//{0x10,6,105,2,1,"Target Evap. Temp."},
//{0x10,8,105,2,1,"Target Cond. Temp."},
//{0x10,10,307,1,-1,"Discharge Temp. Drop"},
//{0x10,10,310,1,-1,"Discharge Temp. Protection Retry Qty"},
//{0x10,10,303,1,-1,"Comp. INV Current Drop"},
//{0x10,10,311,1,-1,"Comp. INV Current Protection Retry Qty"},
//{0x10,11,307,1,-1,"HP Drop Control"},
//{0x10,11,310,1,-1,"HP Protection Retry Qty"},
//{0x10,11,303,1,-1,"LP Drop Control"},
//{0x10,11,311,1,-1,"LP Protection Retry Qty"},
//{0x10,12,307,1,-1,"Fin Temp. Drop Control"},
//{0x10,12,310,1,-1,"Fin Temp. Protection Retry Qty"},
//{0x10,13,218,1,-1,"Operation control mode"},
//{0x20,0,105,2,1,"R1T-Outdoor air temp."},
//{0x20,2,105,2,1,"R4T (deicer)"},
// Unnamed; from live values probably the subcooling heat exchanger gas side
//{0x20,4,105,2,1,"20-4 (subcool hex gas?)"},
//{0x20,6,105,2,1,"R2T (discharge)"},
// Unnamed; from live values probably a first suction sensor
//{0x20,8,105,2,1,"20-8 (suction 1?)"},
//{0x20,10,105,2,1,"R5T (suction 2)"},
//{0x20,12,105,2,1,"R7T (liquid)"},
//{0x20,14,105,2,2,"Low Pressure"},
//{0x20,14,405,2,1,"Low Pressure(T)"},
//{0x20,16,105,2,2,"High Pressure"},
// R410A curve (801); High Pressure(T) 27.6 C vs Target Cond. 28.2 C on the live unit
//{0x20,16,405,2,1,"High Pressure(T)"},
// Unnamed; from live values probably a second liquid sensor
//{0x20,18,105,2,1,"20-18 (liquid 2?)"},
// 0x30 (10 bytes): compressor, fan, expansion valves, 4-way valve, crankcase heater, solenoid valve
//{0x30,0,152,1,-1,"INV Comp. Frequency(Hz)"},
//{0x30,1,211,1,-1,"Fan 1 (step)"},
//{0x30,2,151,2,-1,"EV1 main (pls)"},
//{0x30,4,151,2,-1,"EV3 subcool (pls)"},
//{0x30,6,307,1,-1,"4 Way Valve"},
// Inferred from the 0x00 quantities, not checked live
//{0x30,7,307,1,-1,"Crank case heater 1"},
//{0x30,8,306,1,-1,"SV (liquid pipe shut)"},
// The hydrobox, as the outdoor unit sees it (read with its address)
//{0x50,1,152,1,-1,"Hydrobox Address"},
//{0x50,2,313,1,-1,"I/U operation mode"},
//{0x50,2,303,1,-1,"I/U Thermostat ON/OFF"},
//{0x50,2,302,1,-1,"Freeze Protection"},
//{0x50,3,204,1,-1,"I/U Error Code"},
//{0x50,4,314,2,-1,"Hydrobox Model Code"},
// 160 on an EKHBRD016
//{0x50,6,152,1,-1,"Hydrobox capacity code"},
//{0x50,9,107,2,1,"Inlet water temp. (hydrobox)"},
//{0x50,11,107,2,1,"R3T (R410A liquid)"},
//{0x50,13,107,2,1,"Leaving water temp. (hydrobox)"},
//{0x50,15,151,2,-1,"K1E (R410A EEV)(pls)"},
};
