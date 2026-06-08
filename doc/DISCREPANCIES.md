# Daikin Definition Discrepancies Report
================================================================================

This report lists labels that have DIFFERENT mappings across model files.
Same label name, but different registry/offset/convid/size/type values.

Total discrepancies found: 72

================================================================================

## "*Refrigerant type"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x00, 0, 802, 0, -1}
    Used in 20 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 15 more
  Variant 2 
    Mapping: {0x00, 0, 801, 0, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more

## "4 Way Valve"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 11, 307, 1, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 10 more
  Variant 2 
    Mapping: {0x30, 0, 307, 1, -1}
    Used in 3 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h

## "4 Way Valve 1"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 307, 1, -1}
    Used in 7 file(s):
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_Multi_DHWHP).h
      ... and 2 more
  Variant 2 
    Mapping: {0x30, 7, 307, 1, -1}
    Used in 6 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      ... and 1 more
  Variant 3 
    Mapping: {0x30, 11, 307, 1, -1}
    Used in 1 file(s):
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h

## "4 Way Valve 2"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 306, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x30, 7, 306, 1, -1}
    Used in 3 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "4 Way Valve 3"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 305, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x30, 7, 305, 1, -1}
    Used in 3 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "4 Way Valve 4"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 304, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x30, 7, 304, 1, -1}
    Used in 3 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "4 Way Valve 5"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 303, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x30, 7, 303, 1, -1}
    Used in 3 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "4 way valve (Y1S)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 12, 301, 1, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 2 
    Mapping: {0x30, 13, 307, 1, -1}
    Used in 1 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
  Variant 3 
    Mapping: {0xA0, 12, 301, 1, 1}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "Add pump"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x64, 9, 302, 1, -1}
    Used in 23 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 18 more
  Variant 2 
    Mapping: {0x64, 9, 301, 1, -1}
    Used in 4 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h
      - Daikin Mini chiller(EWAA-EWYA D series 9-16kW).h
  Variant 3 
    Mapping: {0x64, 2, 302, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "BUH output capacity"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x63, 13, 311, 1, -1}
    Used in 8 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      ... and 3 more
  Variant 2 
    Mapping: {0x63, 13, 152, 1, -1}
    Used in 7 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      - Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h
      - Altherma(EPRA E ETSH-X 16P30-50 E series 8-12kW-ECH2O).h
      - Altherma(EPRA E ETV16-ETB16-ETVZ16 E_EJ series 8-12kW).h
      ... and 2 more

## "Brine inlet temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 7, 105, 2, 1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x21, 4, 105, 2, 1}
    Used in 2 file(s):
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h
  Variant 3 
    Mapping: {0x21, 6, 105, 2, 1}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "Brine outlet temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 9, 105, 2, 1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x21, 6, 105, 2, 1}
    Used in 2 file(s):
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h
  Variant 3 
    Mapping: {0x21, 8, 105, 2, 1}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "Compressor outlet temperature"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 10, 105, 2, 1}
    Used in 16 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 11 more
  Variant 2 
    Mapping: {0x21, 10, 152, 2, 1}
    Used in 1 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h

## "Crank case heater"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 12, 307, 1, -1}
    Used in 16 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 11 more
  Variant 2 
    Mapping: {0xA0, 12, 305, 1, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 3 
    Mapping: {0x30, 0, 307, 1, -1}
    Used in 3 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h
  Variant 4 
    Mapping: {0x30, 8, 307, 1, -1}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "Crank case heater 1"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 307, 1, -1}
    Used in 10 file(s):
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      ... and 5 more
  Variant 2 
    Mapping: {0x30, 8, 307, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "Crank case heater 2"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 306, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x30, 8, 306, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "Crank case heater 3"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 305, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x30, 8, 305, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "Crank case heater 4"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 304, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x30, 8, 304, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "DHW priority request"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x62, 8, 304, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_Multi_DHWHP).h
      - Altherma(LT_Multi_Hybrid).h
  Variant 2 
    Mapping: {0x64, 2, 304, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "DHW:SV1"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 13, 307, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h
  Variant 2 
    Mapping: {0xA0, 12, 302, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "Data Enable/Disable"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x60, 0, 304, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 2 
    Mapping: {0x61, 0, 307, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 3 
    Mapping: {0x62, 0, 307, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 4 
    Mapping: {0x63, 0, 307, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 5 
    Mapping: {0x64, 0, 307, 1, -1}
    Used in 31 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 26 more
  Variant 6 
    Mapping: {0x65, 0, 307, 1, -1}
    Used in 8 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      - Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h
      - Altherma(EPRA E ETSH-X 16P30-50 E series 8-12kW-ECH2O).h
      - Altherma(EPRA E ETV16-ETB16-ETVZ16 E_EJ series 8-12kW).h
      ... and 3 more

## "Discharge Temp. Drop"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x10, 10, 307, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 2 
    Mapping: {0xA0, 13, 303, 1, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more

## "Drain pan heater"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 13, 306, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h
  Variant 2 
    Mapping: {0xA0, 12, 303, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "Error Code"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x10, 5, 204, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 2 
    Mapping: {0x60, 3, 204, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more

## "Error type"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x10, 4, 203, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 2 
    Mapping: {0x60, 5, 203, 1, -1}
    Used in 22 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 17 more

## "Expansion valve 1 (pls)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 3, 151, 2, -1}
    Used in 19 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 14 more
  Variant 2 
    Mapping: {0x30, 0, 151, 2, -1}
    Used in 3 file(s):
      - Altherma(Hybrid).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Daikin Mini chiller(EWAA-EWYA D series 9-16kW).h

## "Expansion valve 2 (pls)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 5, 151, 2, -1}
    Used in 19 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 14 more
  Variant 2 
    Mapping: {0x30, 2, 151, 2, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more

## "Expansion valve 3 (pls)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 7, 151, 2, -1}
    Used in 16 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 11 more
  Variant 2 
    Mapping: {0xA0, 8, 151, 2, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 3 
    Mapping: {0x30, 4, 151, 2, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more

## "Expansion valve 4 (pls)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 9, 151, 2, -1}
    Used in 16 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 11 more
  Variant 2 
    Mapping: {0x30, 6, 151, 2, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more

## "Ext. indoor ambient sensor (R6T)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x61, 14, 105, 2, 1}
    Used in 26 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 21 more
  Variant 2 
    Mapping: {0x61, 13, 105, 2, 1}
    Used in 2 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h

## "Fan 1 (10 rpm)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 211, 1, -1}
    Used in 4 file(s):
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
  Variant 2 
    Mapping: {0x30, 1, 211, 1, -1}
    Used in 1 file(s):
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h

## "Fan 1 (step)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 1, 211, 1, -1}
    Used in 17 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 12 more
  Variant 2 
    Mapping: {0x30, 0, 211, 1, -1}
    Used in 9 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      ... and 4 more

## "Fan 2 (step)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 2, 211, 1, -1}
    Used in 19 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 14 more
  Variant 2 
    Mapping: {0x30, 1, 211, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more

## "Fan1 Fin temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 6, 105, 2, 1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 2 
    Mapping: {0x21, 6, 152, 2, 1}
    Used in 1 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h

## "Fan2 Fin temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 8, 105, 2, 1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 2 
    Mapping: {0x21, 8, 152, 2, 1}
    Used in 1 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h

## "I/U EEPROM (12th digit)(rev.)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x63, 7, 214, 1, -1}
    Used in 17 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      ... and 12 more
  Variant 2 
    Mapping: {0x63, 7, 215, 1, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more

## "INV fin temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 4, 105, 2, 1}
    Used in 17 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 12 more
  Variant 2 
    Mapping: {0x20, 12, 105, 2, 1}
    Used in 1 file(s):
      - Altherma(Hybrid).h

## "INV secondary current (A)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 2, 105, 2, -1}
    Used in 30 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 25 more
  Variant 2 
    Mapping: {0x21, 0, 105, 2, -1}
    Used in 2 file(s):
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h

## "Indoor Unit Address"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x60, 1, 152, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 2 
    Mapping: {0x61, 1, 152, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 3 
    Mapping: {0x62, 1, 152, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 4 
    Mapping: {0x63, 1, 152, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 5 
    Mapping: {0x64, 1, 152, 1, -1}
    Used in 31 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 26 more
  Variant 6 
    Mapping: {0x65, 0, 152, 1, -1}
    Used in 4 file(s):
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      - Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h
      - Altherma(EPRA E ETSH-X 16P30-50 E series 8-12kW-ECH2O).h
      - Altherma(EPRA E ETV16-ETB16-ETVZ16 E_EJ series 8-12kW).h
  Variant 7 
    Mapping: {0x65, 1, 152, 2, -1}
    Used in 3 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(ERLA D EBSH-X 16P30-50 D SERIES 11-16kW-ECH2O).h
      - Altherma(ERLA D EBV-EBB-EBVZ D SERIES 11-16kW).h
  Variant 8 
    Mapping: {0x65, 1, 152, 1, -1}
    Used in 1 file(s):
      - Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h

## "Indoor Unit Code"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x60, 4, 314, 2, -1}
    Used in 8 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      ... and 3 more
  Variant 2 
    Mapping: {0x60, 4, 152, 2, -1}
    Used in 2 file(s):
      - Altherma(LT_Multi_DHWHP).h
      - Altherma(LT_Multi_Hybrid).h

## "Liquid pipe temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 4, 119, 2, 1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 2 
    Mapping: {0x20, 10, 105, 2, 1}
    Used in 3 file(s):
      - Altherma(Hybrid).h
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h

## "Low Pressure(T)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x20, 14, 405, 2, 1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 10 more
  Variant 2 
    Mapping: {0x20, 14, 405, 2, -1}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "Main pump"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x64, 9, 301, 1, -1}
    Used in 23 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 18 more
  Variant 2 
    Mapping: {0x64, 9, 300, 1, -1}
    Used in 4 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h
      - Daikin Mini chiller(EWAA-EWYA D series 9-16kW).h
  Variant 3 
    Mapping: {0x64, 2, 301, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "Mixed water temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x64, 10, 118, 2, 1}
    Used in 18 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 13 more
  Variant 2 
    Mapping: {0x64, 10, 105, 2, 1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "Mixed water temp.(R7T)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x64, 10, 118, 2, 1}
    Used in 8 file(s):
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h
      ... and 3 more
  Variant 2 
    Mapping: {0x63, 8, 105, 2, 1}
    Used in 1 file(s):
      - Altherma(ERLA D EBV-EBB-EBVZ D SERIES 11-16kW).h

## "Not in use"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x10, 12, 311, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 2 
    Mapping: {0x62, 2, 301, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 3 
    Mapping: {0x62, 8, 306, 1, -1}
    Used in 23 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 18 more
  Variant 4 
    Mapping: {0x62, 8, 305, 1, -1}
    Used in 23 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 18 more
  Variant 5 
    Mapping: {0x60, 11, 302, 1, -1}
    Used in 18 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      ... and 13 more
  Variant 6 
    Mapping: {0x60, 11, 301, 1, -1}
    Used in 18 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      ... and 13 more
  Variant 7 
    Mapping: {0x21, 15, 105, 1, -1}
    Used in 14 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      ... and 9 more
  Variant 8 
    Mapping: {0x21, 16, 105, 1, -1}
    Used in 14 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      ... and 9 more
  Variant 9 
    Mapping: {0x21, 17, 105, 1, -1}
    Used in 14 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      ... and 9 more
  Variant 10 
    Mapping: {0x21, 18, 105, 1, -1}
    Used in 14 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      ... and 9 more
  Variant 11 
    Mapping: {0x21, 6, 306, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 12 
    Mapping: {0x21, 6, 305, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 13 
    Mapping: {0x21, 6, 304, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 14 
    Mapping: {0x21, 6, 303, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 15 
    Mapping: {0x21, 6, 302, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 16 
    Mapping: {0x21, 6, 301, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 17 
    Mapping: {0x21, 6, 300, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 18 
    Mapping: {0x62, 8, 304, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 19 
    Mapping: {0x62, 15, 152, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 20 
    Mapping: {0x62, 16, 307, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 21 
    Mapping: {0x62, 16, 306, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 22 
    Mapping: {0x62, 16, 305, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 23 
    Mapping: {0x62, 16, 304, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 24 
    Mapping: {0x62, 16, 303, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 25 
    Mapping: {0x62, 16, 302, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 26 
    Mapping: {0x62, 16, 301, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 27 
    Mapping: {0x62, 16, 300, 1, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 7 more
  Variant 28 
    Mapping: {0x63, 8, 215, 1, -1}
    Used in 8 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      ... and 3 more
  Variant 29 
    Mapping: {0x63, 9, 215, 1, -1}
    Used in 8 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      ... and 3 more
  Variant 30 
    Mapping: {0x63, 10, 215, 1, -1}
    Used in 8 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      ... and 3 more
  Variant 31 
    Mapping: {0x20, 16, 105, 2, -1}
    Used in 6 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h
      ... and 1 more
  Variant 32 
    Mapping: {0x20, 18, 105, 2, -1}
    Used in 6 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_11-16kW Bizone).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h
      ... and 1 more
  Variant 33 
    Mapping: {0x63, 11, 215, 1, -1}
    Used in 4 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - EKHWET-BAV3(Multi DHW tank).h
  Variant 34 
    Mapping: {0x63, 12, 215, 1, -1}
    Used in 3 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - EKHWET-BAV3(Multi DHW tank).h
  Variant 35 
    Mapping: {0x62, 8, 303, 1, -1}
    Used in 1 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h

## "O/U MPU ID"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 10, 152, 1, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 2 
    Mapping: {0xA0, 11, 152, 1, -1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more

## "Outdoor heat exchanger temp."
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 2, 119, 2, 1}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 2 
    Mapping: {0x20, 2, 105, 2, 1}
    Used in 2 file(s):
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h

## "Outlet water heat exchanger temp (hydro split model) DLWB2"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x65, 2, 105, 2, 1}
    Used in 8 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      - Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h
      - Altherma(EPRA E ETSH-X 16P30-50 E series 8-12kW-ECH2O).h
      - Altherma(EPRA E ETV16-ETB16-ETVZ16 E_EJ series 8-12kW).h
      ... and 3 more
  Variant 2 
    Mapping: {0x30, 11, 112, 1, 1}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "PHE Heater"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x62, 8, 304, 1, -1}
    Used in 17 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      ... and 12 more
  Variant 2 
    Mapping: {0xA1, 4, 300, 1, -1}
    Used in 16 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 11 more

## "Powerful demand"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x62, 8, 305, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_Multi_DHWHP).h
      - Altherma(LT_Multi_Hybrid).h
  Variant 2 
    Mapping: {0x64, 2, 305, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "Pressure"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 6, 119, 2, 2}
    Used in 15 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 10 more
  Variant 2 
    Mapping: {0x20, 14, 105, 2, 2}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 8 more

## "Pressure sensor(T)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x62, 15, 405, 2, 1}
    Used in 8 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      - Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h
      ... and 3 more
  Variant 2 
    Mapping: {0x20, 14, 405, 2, 1}
    Used in 3 file(s):
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h

## "RA:SV2"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 13, 305, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h
  Variant 2 
    Mapping: {0xA0, 12, 304, 1, -1}
    Used in 1 file(s):
      - EKHWET-BAV3(Multi DHW tank).h

## "Refrig. temp. evap. In"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 11, 105, 2, 1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x21, 8, 105, 2, 1}
    Used in 2 file(s):
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h

## "Refrig. temp. evap.Out"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 13, 105, 2, 1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 2 
    Mapping: {0x21, 10, 105, 2, 1}
    Used in 2 file(s):
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h
  Variant 3 
    Mapping: {0x21, 12, 105, 2, 1}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "Refrigerant pressure sensor"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x62, 15, 105, 2, 2}
    Used in 19 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 14 more
  Variant 2 
    Mapping: {0x62, 15, 152, 2, 2}
    Used in 1 file(s):
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h

## "SV (drain pan heater)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 13, 306, 1, -1}
    Used in 5 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      - Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h
      - Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW).h
      - Daikin Mini chiller(EWAA-EWYA D series 4-8kW).h
  Variant 2 
    Mapping: {0xA0, 12, 303, 1, -1}
    Used in 5 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      - Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h
      - Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW).h
      - Daikin Mini chiller(EWAA-EWYA D series 4-8kW).h

## "SV1 (hot gas)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 9, 307, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h
  Variant 2 
    Mapping: {0x30, 0, 307, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CB_11-16kW Bizone).h
      - Daikin Mini chiller(EWAA-EWYA D series 9-16kW).h

## "SV2 (BPH)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 9, 306, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h
  Variant 2 
    Mapping: {0x30, 0, 306, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CB_11-16kW Bizone).h
      - Daikin Mini chiller(EWAA-EWYA D series 9-16kW).h

## "Solenoid Valve 1"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 12, 302, 1, -1}
    Used in 14 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 9 more
  Variant 2 
    Mapping: {0x30, 0, 307, 1, -1}
    Used in 10 file(s):
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      - Altherma(LT_CA_CB_04-08kW).h
      ... and 5 more

## "Solenoid Valve 2"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 12, 303, 1, -1}
    Used in 9 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      - Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h
      ... and 4 more
  Variant 2 
    Mapping: {0x30, 0, 306, 1, -1}
    Used in 9 file(s):
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(LT_CA_CB_04-08kW).h
      - Altherma(LT_CB_04-08kW Bizone).h
      ... and 4 more

## "Solenoid Valve 3"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0xA0, 12, 304, 1, -1}
    Used in 14 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      ... and 9 more
  Variant 2 
    Mapping: {0x30, 0, 305, 1, -1}
    Used in 13 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(Hybrid).h
      ... and 8 more
  Variant 3 
    Mapping: {0x30, 9, 305, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "Solenoid Valve 4"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 304, 1, -1}
    Used in 14 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      ... and 9 more
  Variant 2 
    Mapping: {0x30, 9, 304, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "Solenoid Valve 5"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x30, 0, 303, 1, -1}
    Used in 14 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      ... and 9 more
  Variant 2 
    Mapping: {0x30, 9, 303, 1, -1}
    Used in 2 file(s):
      - Altherma(LT_CA_CB_11-16kW).h
      - Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h

## "Target delta T cooling"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x64, 15, 152, 1, 1}
    Used in 19 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 14 more
  Variant 2 
    Mapping: {0x64, 15, 152, 1, -1}
    Used in 1 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h

## "Target delta T heating"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x64, 14, 152, 1, 1}
    Used in 19 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 14 more
  Variant 2 
    Mapping: {0x64, 14, 152, 1, -1}
    Used in 1 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h

## "Thermostat ON/OFF"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x10, 1, 307, 1, -1}
    Used in 32 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 27 more
  Variant 2 
    Mapping: {0x60, 2, 303, 1, -1}
    Used in 30 file(s):
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      - Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h
      - Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h
      ... and 25 more

## "Voltage (N-phase) (V)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x21, 4, 101, 2, -1}
    Used in 12 file(s):
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h
      - Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h
      - Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h
      - Altherma(LT_CA_CB_04-08kW).h
      ... and 7 more
  Variant 2 
    Mapping: {0x21, 2, 101, 2, -1}
    Used in 2 file(s):
      - Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h
      - Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h

## "Water pressure"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x62, 11, 105, 1, 2}
    Used in 29 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h
      - Altherma(EGSQH-A series 10kW GEO2).h
      - Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h
      ... and 24 more
  Variant 2 
    Mapping: {0x62, 11, 405, 1, 1}
    Used in 3 file(s):
      - Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h
      - Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW).h
      - Daikin Mini chiller(EWAA-EWYA D series 4-8kW).h

## "[EKMIK] Bizone kit mix valve position M1S"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x65, 6, 152, 1, -1}
    Used in 5 file(s):
      - Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h
      - Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h
      - Altherma(EPRA E ETSH-X 16P30-50 E series 8-12kW-ECH2O).h
      - Altherma(EPRA E ETV16-ETB16-ETVZ16 E_EJ series 8-12kW).h
      - Altherma(ERLA D EBV-EBB-EBVZ D SERIES 11-16kW).h
  Variant 2 
    Mapping: {0x65, 6, 101, 1, -1}
    Used in 3 file(s):
      - Altherma(EBLA-EDLA D series 9-16kW Monobloc).h
      - Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h
      - Altherma(ERLA D EBSH-X 16P30-50 D SERIES 11-16kW-ECH2O).h

## "[RT space thermo ON/OFF] (bit1)"
------------------------------------------------------------
  Variant 1 [MOST COMMON]
    Mapping: {0x63, 11, 301, 1, -1}
    Used in 3 file(s):
      - Altherma(EBLA-EDLA D series 4-8kW Monobloc).h
      - Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h
      - Daikin Mini chiller(EWAA-EWYA D series 4-8kW).h
  Variant 2 
    Mapping: {0x63, 12, 301, 1, -1}
    Used in 1 file(s):
      - Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW).h
