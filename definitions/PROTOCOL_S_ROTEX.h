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


    // The names carry no "(C)": the unit is the type (Home Assistant shows °C). Capitalised like the
    // Altherma files, "temp." in lower case.
    // The temperatures are signed: the refrigerant goes below 0 during a defrost. Type 1: Home Assistant gets
    // them as temperatures (°C, statistics).
    {0x54, 0, 103, 2, 1, "Refrig. temp. liquid side"},
    {0x54, 2, 103, 2, 1, "Inlet water temp."},
    {0x54, 4, 103, 2, 1, "Outlet water temp."},
    // Was "D(C)": the leaving water temperature the remote controller shows (RKHBH008BA with ARC448A3, by TungstenE2)
    {0x54, 6, 103, 2, 1, "Leaving water temp. after BUH (R2T)"},
    {0x54, 8, 103, 2, 1, "DHW tank temp."},
    {0x54, 10, 103, 2, -1, "F (unknown)"},
    // Was "Delta-Tr(deg)": the weather-dependent leaving water target, whole degrees, falls when the outdoor
    // temperature rises (RKHBH008BA with ARC448A3, by TungstenE2). Named as on the Altherma files, so the
    // drawing shows it as the leaving water target.
    {0x54, 12, 101, 1, 1, "LW setpoint (main)"},
    {0x54, 13, 151, 1, -1, "R/C setpoint"},

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
