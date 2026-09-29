#include "labeldef.h"
//  This file is a definition file for Rotex heat pump, technically looking like protocol S, but not using the same mapping.
//  Confirmed working for Rotex RDLQ014AA6V3.

LabelDef labelDefs[] = {
    // 0x50 not supported by RDLQ014AA6V3

    // 0x53 seems to be a collection of booleans
    {0x53, 0, 200, 1, -1, "Circulation pump"},
    {0x53, 3, 200, 1, -1, "External heater?"},
    {0x53, 5, 200, 1, -1, "Priority to domestic water"},
    {0x53, 6, 200, 1, -1, "Burner inhibit from solaris"},


    // Testing stuff....
    {0x54, 0, 153, 2, -1, "Refrig. Temp. liquid side(C)"},
    {0x54, 2, 153, 2, -1, "Inlet water temp.(C)"},
    {0x54, 4, 153, 2, -1, "Outlet Water Temp.(C)"},
    {0x54, 6, 153, 2, -1, "D(C)"},
    {0x54, 8, 153, 2, -1, "DHW tank temp.(C)"},
    {0x54, 10, 103, 2, -1, "F(C)"},
    // Byte 12 seems to be a 1 byte value, evolving during day in continuous way
    // 0x1b mid-day, 0x24 mid night -> some delta temp??
    {0x54, 12, 101, 1, -1, "Delta-Tr(deg)"},
    {0x54, 13, 151, 1, -1, "R/C Setpoint(C)"},

    {0x55, 0, 201, 1, -1, "Operation Mode"},
    {0x55, 1, 204, 1, -1, "Error Code"},
    {0x55, 2, 204, 1, -1, "Thermo Off Error"},
    {0x55, 3, 204, 1, -1, "Warning Code"},
    {0x55, 4, 204, 1, -1, "Caution Code"},
    // TODO: seems to reply on 0x56 as well, but not used by dchecker...
    {0x56, 0, 103, 2, -1, "????"},
};

// Override protocol
#define PROTOCOL 'S'
