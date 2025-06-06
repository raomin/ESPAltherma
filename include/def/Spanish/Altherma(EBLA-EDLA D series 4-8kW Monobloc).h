#include "labeldef.h"
//  This file is a definition file for ESPAtherma
//  uncomment each value you want to query for your installation.


LabelDef labelDefs[] = {
//{0x00,0,802,0,-1,"*Tipo de refrigerante"},
//{0x00,0,152,1,-1,"Cant. de datos del sensor"},
//{0x00,1,152,1,-1,"Cant. de compresor INV"},
//{0x00,2,152,1,-1,"Cant. de compresores STD"},
//{0x00,3,152,1,-1,"Cant. de datos de ventilador"},
//{0x00,4,152,1,-1,"Cant. de datos de la válvula de expansión"},
//{0x00,5,152,1,-1,"Cant. de datos de la válvula de 4 vías"},
//{0x00,6,152,1,-1,"Cant. de calentadores del cárter"},
//{0x00,7,152,1,-1,"Cant. de válvulas de solenoide"},
//{0x00,8,152,1,-1,"Unidades interiores conectable máx."},
//{0x00,9,152,1,-1,"Cant. de unidades interiores conectadas"},
//{0x00,10,152,1,-1,"U/ext. MPU ID (xx)"},
//{0x00,11,152,1,-1,"U/ext. MPU ID (yy)"},
//{0x00,12,105,1,-1,"Capacidad U/Ext (kW)"},
//{0x10,0,217,1,-1,"Modo de funcionamiento"},
//{0x10,1,307,1,-1,"Termostato ON/OFF"},
//{0x10,1,306,1,-1,"Anticiclo reinicio"},
//{0x10,1,305,1,-1,"Control de arranque"},
//{0x10,1,304,1,-1,"Desescarche"},
//{0x10,1,303,1,-1,"Retorno de aceite"},
//{0x10,1,302,1,-1,"Compensación de presiones"},
//{0x10,1,301,1,-1,"Señal de demanda"},
//{0x10,1,300,1,-1,"Control reducción de ruido"},
//{0x10,4,203,1,-1,"Tipo de avería"},
//{0x10,5,204,1,-1,"Código de error"},
//{0x10,6,114,2,1,"Temp. de evaporación objetivo"},
//{0x10,8,114,2,1,"Temp. de condensación objetivo"},
//{0x10,10,307,1,-1,"Caída de la temperatura de descarga"},
//{0x10,10,310,1,-1,"Reintentos protección temp. descarga"},
//{0x10,10,303,1,-1,"Caida de corriente compresor INV"},
//{0x10,10,311,1,-1,"Reintentos protección corriente INV"},
//{0x10,11,307,1,-1,"HP drop control"},
//{0x10,11,310,1,-1,"Reintentos protección HP"},
//{0x10,11,303,1,-1,"Control caida Baja Presión"},
//{0x10,11,311,1,-1,"Reintentos proteccion Baja Presión"},
//{0x10,12,307,1,-1,"Control caida temp. Frec INV"},
//{0x10,12,310,1,-1,"Reintentos protección temp. Frec INV"},
//{0x10,12,303,1,-1,"Otro de control de caída"},
//{0x10,12,311,1,-1,"No se utiliza"},
//{0x11,0,215,1,-1,"U/Ext. EEPROM (1er dígito)"},
//{0x11,1,215,1,-1,"U/Ext. EEPROM (3er 4º dígito)"},
//{0x11,2,215,1,-1,"U/Ext. EEPROM (5º 6º dígito)"},
//{0x11,3,215,1,-1,"U/Ext. EEPROM (7º 8º dígito)"},
//{0x11,4,215,1,-1,"U/Ext. EEPROM (10º dígito)"},
//{0x11,5,214,1,-1,"U/Ext. EEPROM (11º dígito)"},
//{0x00,0,995,1,-1,"NextDataGrid"},
//{0x20,0,105,2,1,"Temp. Ambiente exterior (R1T)"},
//{0x20,2,105,2,1,"Temp. Intercambiador U/Ext"},
//{0x20,4,105,2,1,"Temp. del tubo de descarga"},
//{0x20,6,105,2,1,"Temp. Tubo de aspiración"},
//{0x20,8,105,2,1,"Temp. Media Intercambiador U/Ext"},
//{0x20,10,105,2,1,"Temperatura del líquido (R3T)"},
//{0x20,12,105,2,2,"Presión alta"},
//{0x20,12,405,2,1,"Presión alta(T)"},
//{0x20,14,105,2,2,"Presión baja"},
//{0x20,14,405,2,1,"Presión baja(T)"},
//{0x21,0,105,2,-1,"Corriente primaria del INV (A)"},
//{0x21,2,105,2,-1,"Corriente secundaria del INV (A)"},
//{0x21,4,105,2,1,"Temperatura de la aleta del INV"},
//{0x21,6,105,2,1,"Fan1 Fin temp."},
//{0x21,8,105,2,1,"Fan2 Fin temp."},
//{0x21,10,105,2,1,"Temperatura de salida del compresor"},
//{0x00,0,995,1,-1,"NextDataGrid"},
//{0x30,0,152,1,-1,"Frecuencia del INV (rps)"},
//{0x30,1,211,1,-1,"Ventilador 1 (nivel)"},
//{0x30,2,211,1,-1,"Ventilador 2 (nivel)"},
//{0x30,3,151,2,-1,"Válvula de expansión1 (pls)"},
//{0x30,5,151,2,-1,"Válvula de expansión2 (pls)"},
//{0x30,7,151,2,-1,"Válvula de expansión3 (pls)"},
//{0x30,9,151,2,-1,"Válvula de expansión4 (pls)"},
//{0x30,11,307,1,-1,"Válvula de 4 vías"},
//{0x30,12,307,1,-1,"Calentador del cárter"},
//{0x30,13,307,1,-1,"Y1S"},
//{0x30,13,306,1,-1,"SV (drain pan heater)"},
//{0x30,13,305,1,-1,"Y3S"},
//{0x00,0,998,1,-1,"In-Out separator"},
//{0xA0,0,119,2,1,"Temp. Tubo de aspiración"},
//{0xA0,2,119,2,1,"Temperatura del intercambiador de calor exterior"},
//{0xA0,4,119,2,1,"Temp. tubo de líquido"},
//{0xA0,6,119,2,2,"Presión"},
//{0xA0,8,151,2,-1,"Válvula de expansión3 (pls)"},
//{0xA0,10,152,1,-1,"U/ext. MPU ID"},
//{0xA0,11,152,1,-1,"U/ext. MPU ID"},
//{0xA0,12,307,1,-1,"HPS activado"},
//{0xA0,12,306,1,-1,"Operación de salvaguardia"},
//{0xA0,12,305,1,-1,"Calentador del cárter"},
//{0xA0,12,304,1,-1,"Válvula solenoide 3"},
//{0xA0,12,303,1,-1,"SV (drain pan heater)"},
//{0xA0,12,302,1,-1,"Válvula solenoide 1"},
//{0xA0,12,301,1,-1,"Válvula de 4 vías (Y1S)"},
//{0xA0,12,300,1,-1,"52C Output"},
//{0xA0,13,303,1,-1,"Caída de la temperatura de descarga"},
//{0xA0,13,302,1,-1,"Durante la operación de emergencia"},
//{0xA0,13,301,1,-1,"Indicador de explosión de la unidad interior a 50 ° C"},
//{0xA0,13,300,1,-1,"Bit potente (bit de ajuste MT)"},
//{0xA0,14,105,2,1,"Temperatura del puerto del compresor"},
//{0xA1,0,119,2,1,"(Datos brutos)Temperatura de entrada del intercambiador de calor de agua"},
//{0xA1,2,119,2,1,"(Datos brutos)Temperatura de salida del intercambiador de calor de agua."},
//{0xA1,4,302,1,-1,"Electroválvula de líquido INJ (Y4S)"},
//{0xA1,4,301,1,-1,"Calentador de la placa inferior"},
//{0xA1,4,300,1,-1,"Calentador de PHE"},
//{0xA1,5,114,2,1,"Temperatura de descarga objetivo"},
//{0xA1,7,114,2,1,"Temperatura objetivo del puerto"},
//{0xA1,9,305,1,-1,"Ajuste Monobloc"},
//{0xA1,9,304,1,-1,"Ajuste Minichiller"},
//{0xA1,9,303,1,-1,"Ajuste MT"},
//{0xA1,9,302,1,-1,"Ajuste GSHP"},
//{0xA1,9,301,1,-1,"Ajuste Hydro split"},
//{0xA1,9,300,1,-1,"Ajuste Altherma LT"},
//{0x60,0,304,1,-1,"Activación/desactivación datos"},
//{0x60,1,152,1,-1,"Dirección U/Int."},
//{0x60,2,315,1,-1,"Modo func. U/Int"},
//{0x60,2,303,1,-1,"Termostato ON/OFF"},
//{0x60,2,302,1,-1,"Protección de congelación"},
//{0x60,2,301,1,-1,"Modo silencioso"},
//{0x60,2,300,1,-1,"Protección de congelación tuberías"},
//{0x60,3,204,1,-1,"Código de error"},
//{0x60,4,152,1,-1,"Código de error detallado"},
//{0x60,5,203,1,-1,"Tipo de avería"},
//{0x60,6,219,1,-1,"Código de capacidad U/Int"},
//{0x60,7,105,2,1,"Consigna ACS"},
//{0x60,9,105,2,1,"Consigna TAI (principal)"},
//{0x60,11,307,1,-1,"Conmutador de flujo del agua"},
//{0x60,11,306,1,-1,"Protector térmico (Q1L) BUH"},
//{0x60,11,305,1,-1,"Protector térmico BSH"},
//{0x60,11,304,1,-1,"Modo tarifa reducida"},
//{0x60,11,303,1,-1,"Entrada solar"},
//{0x60,11,302,1,-1,"Not translated yet"},
//{0x60,11,301,1,-1,"Not translated yet"},
//{0x60,11,300,1,-1,"Modo bivalente"},
//{0x60,12,307,1,-1,"Válvula de 2 vías (Encendido:Calentar_Apagado:Enfriar)"},
//{0x60,12,306,1,-1,"Válvula de 3 vías (Encendido:DHW_Apagado:Espacio)"},
//{0x60,12,305,1,-1,"Resistencia BSH"},
//{0x60,12,304,1,-1,"Resistencia BUH etapa 1"},
//{0x60,12,303,1,-1,"Resistencia BUH etapa 2"},
//{0x60,12,302,1,-1,"Válvula de cierre de suelo radiante"},
//{0x60,12,301,1,-1,"Funcionamiento bomba"},
//{0x60,12,300,1,-1,"Salida bomba solar"},
//{0x60,13,152,1,-1,"Código de opción interior"},
//{0x60,15,215,1,-1,"U/Int software ID (xx)"},
//{0x60,14,215,1,-1,"U/Int software ID (yy)"},
//{0x60,16,152,1,-1,"U/Int EEPROM Ver."},
//{0x61,0,307,1,-1,"Activación/desactivación datos"},
//{0x61,1,152,1,-1,"Dirección U/Int."},
//{0x61,2,105,2,1,"Temp. Impulsión antes de BUH (R1T)"},
//{0x61,4,105,2,1,"Temp. Impulsión después de BUH (R2T)"},
//{0x61,6,105,2,1,"Temp. Refrigerante líquido (R3T)"},
//{0x61,8,105,2,1,"Temp. Retorno (R4T)"},
//{0x61,10,105,2,1,"Temp. Deposito ACS (R5T)"},
//{0x61,12,105,2,1,"Temperatura ambiente interior (R1T)"},
//{0x61,14,105,2,1,"Temp. Ambiente Ext/Int opcional (R6T)"},
//{0x62,0,307,1,-1,"Activación/desactivación datos"},
//{0x62,1,152,1,-1,"Dirección U/Int."},
//{0x62,2,307,1,-1,"Recalentamiento ON/OFF"},
//{0x62,2,306,1,-1,"Almacenamiento ECO ON/OFF"},
//{0x62,2,305,1,-1,"Almacenamiento Confort ON/OFF"},
//{0x62,2,304,1,-1,"Modo Powerful ACS (ON/OFF)"},
//{0x62,2,303,1,-1,"Calefacción/Regrigeración (ON/OFF)"},
//{0x62,2,302,1,-1,"Sistema OFF (ON=Sistema apagado)"},
//{0x62,2,301,1,-1,"No se utiliza"},
//{0x62,2,300,1,-1,"Modo emergencia (interior) activado / no activado"},
//{0x62,3,105,2,1,"Consigna TAI (adicional)"},
//{0x62,5,105,2,1,"Consigna TH ambiente"},
//{0x62,7,307,1,-1,"TH Ext. Refrigeración(adicional)"},
//{0x62,7,306,1,-1,"TH Ext. calefacción(adicional)"},
//{0x62,7,305,1,-1,"TH Ext. Refrigeración(principal)"},
//{0x62,7,304,1,-1,"TH Ext. Calefacción(principal)"},
//{0x62,7,303,1,-1,"Límite consumo de energía 4"},
//{0x62,7,302,1,-1,"Límite consumo de energía 3"},
//{0x62,7,301,1,-1,"Límite consumo de energía 2"},
//{0x62,7,300,1,-1,"Límite consumo de energía 1"},
//{0x62,8,307,1,-1,"Ninguno"},
//{0x62,8,306,1,-1,"No se utiliza"},
//{0x62,8,305,1,-1,"No se utiliza"},
//{0x62,8,304,1,-1,"Calentador de PHE"},
//{0x62,8,303,1,-1,"Precalentamiento del depósito ON/OFF"},
//{0x62,8,302,1,-1,"Salida bomba ACS"},
//{0x62,8,301,1,-1,"Salida de alarma"},
//{0x62,8,300,1,-1,"Salida calefacción/refrigeración"},
//{0x62,9,105,2,-1,"Caudalímetro (l/min)"},
//{0x62,11,105,1,2,"Presión de agua"},
//{0x62,12,152,1,-1,"Señal bomba (0:máx-100stop)"},
//{0x62,13,152,1,-1,"V3V mezcladora 1"},
//{0x62,14,152,1,-1,"V3V mezcladora 2"},
//{0x62,15,105,2,2,"Sensor de presión de refrigerante"},
//{0x62,15,405,2,1,"Sensor de presión(T)"},
//{0x63,0,307,1,-1,"Activación/desactivación datos"},
//{0x63,1,152,1,-1,"Dirección U/Int."},
//{0x63,2,215,1,-1,"U/Int. EEPROM (3er dígito)"},
//{0x63,3,215,1,-1,"U/Int. EEPROM (4º 5º dígito)"},
//{0x63,4,215,1,-1,"U/Int. EEPROM (6º 7º dígito)"},
//{0x63,5,215,1,-1,"U/Int. EEPROM (8º 9º dígito)"},
//{0x63,6,215,1,-1,"U/Int. EEPROM (11º dígito)"},
//{0x63,7,215,1,-1,"U/Int. EEPROM (12º dígito)(rev.)"},
//{0x63,8,215,1,-1,"No se utiliza"},
//{0x63,9,215,1,-1,"No se utiliza"},
//{0x63,10,215,1,-1,"No se utiliza"},
//{0x63,11,301,1,-1,"Not translated yet"},
//{0x63,12,300,1,-1,"Not translated yet"},
//{0x63,13,311,1,-1,"Capacidad de salida BUH"},
//{0x63,14,161,1,-1,"Medición del sensor CT (L1)"},
//{0x63,15,161,1,-1,"Medición del sensor CT (L2)"},
//{0x63,16,307,1,-1,"HP forzado FG"},
//{0x63,16,161,1,-1,"Medición del sensor CT (L3)"},
//{0x64,0,307,1,-1,"Activación/desactivación datos"},
//{0x64,1,152,1,-1,"Dirección U/Int."},
//{0x64,2,316,1,-1,"Modo Op. Híbrido"},
//{0x64,2,303,1,-1,"Demanda de operación de la caldera"},
//{0x64,2,302,1,-1,"Demanda DWH de caldera"},
//{0x64,2,301,1,-1,"Salida de la válvula de desviación"},
//{0x64,3,105,2,-1,"BE_COP"},
//{0x64,5,105,2,1,"Temperatura objetivo de calentamiento híbrido"},
//{0x64,7,105,2,1,"Temperatura objetivo para calentar la caldera"},
//{0x64,9,302,1,-1,"Add pump"},
//{0x64,9,301,1,-1,"Main pump"},
//{0x64,10,118,2,1,"Temperatura del agua mezclada"},
//{0x64,12,105,2,1,"2nd Domestic hot water temperature"},
//{0x64,14,152,1,1,"Target delta T heating"},
//{0x64,15,152,1,1,"Target delta T cooling"},
};
