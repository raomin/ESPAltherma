#include "labeldef.h"
//  This file is a definition file for ESPAltherma: Daikin Altherma HT, the high temperature hydrobox EKHBRD011-016
//  (R134a second stage) on an ERRQ/ERSQ outdoor unit, read with both protocols in one pass. The board on X41A answers
//  protocol I and protocol S, and neither has everything: I has the inverter currents, S the high pressure and the
//  discharge and liquid temperatures. Here the registries below 0x50 are read with protocol I, exactly as in
//  "Altherma(HT EKHBRD-ERRQ series 11-16kW)", and the registries from 0x50 up with protocol S, as in its
//  "protocol S" twin (S_FROM in scripts/gen_catalog.py). Asked for and specified by the owners in discussion #624.
//  uncomment each value you want to query for your installation.
//
//  The values both protocols give (low pressure, compressor frequency, expansion valve, 4-way valve, error code)
//  are read once, with protocol I: a name is one MQTT key and one Home Assistant entity.
//  Never auto-detected: choose this model from the list.
LabelDef labelDefs[] = {
//{0x00,0,805,0,-1,"*Refrigerant type"},
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
// 0x10 answers the same 16 bytes on every unit and every read, running, stopped or in defrost
// (00 00 00 00 00 00 33 00 00 80 00 00 00 00 00 00): the operation mode (off0), the status flags (off1:
// thermostat, defrost...) and the operation control mode (off13) of the outdoor-style layout always read
// "Fan Only", OFF and "Stop control" here, so they are left out.
//{0x10,4,203,1,-1,"Error type"},
//{0x10,5,204,1,-1,"Error Code"},
// Verified: 5.1, matches the R134a saturation temp. of Low Pressure (stage)
//{0x10,6,105,2,1,"Target Evap. Temp."},
// Always 0x8000 (invalid) on the hydrobox
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
//{0x00,0,995,1,-1,"NextDataGrid"},
// 0x20 answers 17 data bytes. off0/off2 read 0; off4, off6, off10 and off12 barely move (off6 always equals off10)
// and off8 reads -47.3 on every unit (open input): kept unnamed until an owner matches them with something.
//{0x20,4,105,2,1,"20-4 (unknown)"},
//{0x20,6,105,2,1,"20-6 (unknown)"},
//{0x20,10,105,2,1,"20-10 (unknown)"},
//{0x20,12,105,2,1,"20-12 (unknown)"},
// Verified: equals the protocol S low pressure (0x50 off2), kgf/cm2 gauge. Updates with some lag, freezes while stopped
//{0x20,14,105,2,2,"Low Pressure (stage)"},
//{0x20,14,405,2,1,"Low Pressure(T)"},
// 0x21 answers 5 data bytes. off0/off2 follow the compressor: 0 when stopped, about 5-7 at 110-130 Hz
// (named after other definitions using 0x21 off0/off2 for the INV currents, not checked with a clamp meter)
//{0x21,0,105,2,-1,"INV primary current (A)"},
//{0x21,2,105,2,-1,"INV secondary current (A)"},
// 0x30 answers 7 data bytes
// Verified: 0 when stopped, equals the protocol S compressor frequency (0x53 off4)
//{0x30,0,151,2,-1,"INV Comp. Frequency(Hz)"},
// Verified: parks at 500 when stopped, equals the protocol S EV pulses (0x53 off0-1)
//{0x30,2,151,2,-1,"EV (pls)"},
// Verified: ON in defrost, together with the protocol S 4-way output
//{0x30,4,307,1,-1,"4 Way Valve"},
// Verified: switch box cooling fan (M1F/M2F), ON about 9-10 min after compressor start, OFF at stop
//{0x30,5,305,1,-1,"Switch box cooling fan"},
// ---- From here on, protocol S (registries 0x50 and up)
// 0x50: the high pressure of the stage, kgf/cm2 gauge; it collapses to 2-3 during defrost. The low pressure is 0x20's.
//{0x50,0,103,2,2,"High Pressure (stage)"},
//{0x50,0,403,2,1,"High Pressure(T)"},
// 0x53: the actuators protocol I does not report
//{0x53,5,200,1,-1,"Comp. Preheat"},
//{0x53,10,200,1,-1,"Crankcase Heater"},
// 0x54: temperatures of the stage. The liquid temperature follows the water being heated.
//{0x54,8,109,2,1,"Discharge pipe temp."},
//{0x54,10,103,2,1,"Fin Temp."},
//{0x54,14,103,2,1,"R134a liquid temp."},
// 0x55: not checked on the hydrobox yet (the error code is 0x10's)
//{0x55,0,201,1,-1,"Operation Mode"},
//{0x55,2,204,1,-1,"Thermo Off Error"},
//{0x55,3,204,1,-1,"Warning Code"},
//{0x55,4,204,1,-1,"Caution Code"},
};
