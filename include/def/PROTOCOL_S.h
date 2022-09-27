#include "labeldef.h"
//  This file is a definition file for the older protocol S devices.

LabelDef PROGMEM labelDefs[] = {
    {0x50, 0, 103, 2, -1, "HP Sensor(kgcm2)"},
    {0x50, 2, 103, 2, -1, "LP Sensor(kgcm2)"},
    {0x53, 0, 152, 2, -1, "EV (pls)"},
    {0x53, 2, 164, 1, -1, "Outdoor Fan (Upper)(rps)"},
    {0x53, 3, 164, 1, -1, "Outdoor Fan (Lower)(rps)"},
    {0x53, 4, 200, 1, -1, "INV Comp. Frequency(Hz)"},
    {0x53, 5, 200, 1, -1, "Comp. Preheat"},
    {0x53, 6, 200, 1, -1, "52C Output"},
    {0x53, 8, 200, 1, -1, "20R (SV) Output"},
    {0x53, 7, 200, 1, -1, "20S (4-way) Output"},
    {0x53, 10, 200, 1, -1, "Crankcase Heater"},
    {0x53, 11, 200, 1, -1, "Ener-Cut Output"},
    {0x54, 0, 103, 2, -1, "Indoor Suction Air Temp.(C)"},
    {0x54, 2, 103, 2, -1, "Indoor Heat Exchanger Temp.(C)"},
    {0x54, 4, 103, 2, -1, "Outdoor air temp.(C)"},
    {0x54, 6, 103, 2, -1, "Outdoor heat exchanger temp.(C)"},
    {0x54, 8, 109, 2, -1, "Discharge pipe temp.(C)"},
    {0x54, 10, 103, 2, -1, "Fin Temp.(C)"},
    {0x54, 12, 312, 1, -1, "Delta-Tr(deg)"},
    {0x54, 13, 151, 1, -1, "R/C Setpoint(C)"},
    {0x55, 0, 201, 1, -1, "Operation Mode"},
    {0x55, 1, 204, 1, -1, "Error Code"},
    {0x55, 2, 204, 1, -1, "Thermo Off Error"},
    {0x55, 3, 204, 1, -1, "Warning Code"},
    {0x55, 4, 204, 1, -1, "Caution Code"},
};

// Override protocol
#define PROTOCOL 'S'
