#include "labeldef.h"
//  This file is a definition file for ESPAtherma
//  uncomment each value you want to query for your installation.


LabelDef labelDefs[] = {
//{0x00,0,802,0,-1,"*Tipo di refrigerante"},
//{0x00,0,152,1,-1,"Qtà dati sensore"},
//{0x00,1,152,1,-1,"Qtà compressori INV"},
//{0x00,2,152,1,-1,"Qtà compressori STD"},
//{0x00,3,152,1,-1,"Qtà dati ventilatore"},
//{0x00,4,152,1,-1,"Qtà dati valvola di espansione"},
//{0x00,5,152,1,-1,"Qtà dati valvola a 4 vie"},
//{0x00,6,152,1,-1,"Qtà riscaldatori del carter"},
//{0x00,7,152,1,-1,"Qtà elettrovalvole"},
//{0x00,8,152,1,-1,"Qtà max. unità interne collegabili"},
//{0x00,9,152,1,-1,"Qtà unità interne collegate"},
//{0x00,10,152,1,-1,"O/U MPU ID (xx)"},
//{0x00,11,152,1,-1,"O/U MPU ID (yy)"},
//{0x00,12,105,1,-1,"Capacità O/U (kW)"},
//{0x10,0,217,1,-1,"Modo di funzionamento"},
//{0x10,1,307,1,-1,"Termostato ON/OFF"},
//{0x10,1,306,1,-1,"Standby riavvio"},
//{0x10,1,305,1,-1,"Controllo all avviamento"},
//{0x10,1,304,1,-1,"Sbrinamento"},
//{0x10,1,303,1,-1,"Recupero dell’olio"},
//{0x10,1,302,1,-1,"Funzionamento regolazione di pressione"},
//{0x10,1,301,1,-1,"Segnale su richiesta"},
//{0x10,1,300,1,-1,"Controllo bassa rumorosità"},
//{0x10,4,203,1,-1,"Tipo malfunzionamento"},
//{0x10,5,204,1,-1,"Codice malfunzionamento"},
//{0x10,6,114,2,1,"Temp. evap. target"},
//{0x10,8,114,2,1,"Temp. Cond. target"},
//{0x10,10,307,1,-1,"Calo temp. scarico"},
//{0x10,10,310,1,-1,"Qtà tentativi protezione temp. Scarico"},
//{0x10,10,303,1,-1,"Calo corrente INV comp."},
//{0x10,10,311,1,-1,"Qtà tentativi protezione corrente INV comp."},
//{0x10,11,307,1,-1,"Controllo calo HP"},
//{0x10,11,310,1,-1,"Qtà tentativi protezione HP"},
//{0x10,11,303,1,-1,"Controllo calo LP"},
//{0x10,11,311,1,-1,"Qtà tentativi protezione LP"},
//{0x10,12,307,1,-1,"Controllo calo temp. aletta"},
//{0x10,12,310,1,-1,"Qtà tentativi protezione temp. aletta"},
//{0x10,12,303,1,-1,"Altro controllo calo"},
//{0x10,12,311,1,-1,"Non in uso"},
//{0x11,0,215,1,-1,"O/U EEPROM (1º cifra)"},
//{0x11,1,215,1,-1,"O/U EEPROM (3º 4º cifra)"},
//{0x11,2,215,1,-1,"O/U EEPROM (5º 6º cifra)"},
//{0x11,3,215,1,-1,"O/U EEPROM (7º 8º cifre)"},
//{0x11,4,215,1,-1,"O/U EEPROM (10º cifre)"},
//{0x11,5,214,1,-1,"O/U EEPROM (11º cifra)"},
//{0x00,0,995,1,-1,"NextDataGrid"},
//{0x20,0,105,2,1,"Temperatura dell aria esterna(R1T)"},
//{0x20,2,105,2,1,"Temp. scambiatore di calore O/U"},
//{0x20,4,105,2,1,"Temperatura tubo di scarico"},
//{0x20,6,105,2,1,"Temperatura linea di aspirazione"},
//{0x20,8,105,2,1,"Temp. media scambiatore di calore"},
//{0x20,10,105,2,1,"Temperatura liquido (R3T)"},
//{0x20,12,105,2,2,"Alta pressione"},
//{0x20,12,405,2,1,"Alta pressione(T)"},
//{0x20,14,105,2,2,"Bassa pressione"},
//{0x20,14,405,2,1,"Bassa pressione(T)"},
//{0x21,0,105,2,-1,"Corrente primaria INV (A)"},
//{0x21,2,105,2,-1,"Corrente secondaria INV (A)"},
//{0x21,4,105,2,1,"Temp. aletta INV"},
//{0x21,6,105,2,1,"Fan1 Fin temp."},
//{0x21,8,105,2,1,"Fan2 Fin temp."},
//{0x21,10,105,2,1,"Temperatura uscita compressore"},
//{0x00,0,995,1,-1,"NextDataGrid"},
//{0x30,0,152,1,-1,"Frequenza INV (giri/sec.)"},
//{0x30,1,211,1,-1,"Ventilatore 1 (gradino)"},
//{0x30,2,211,1,-1,"Ventilatore 2 (gradino)"},
//{0x30,3,151,2,-1,"Valvola di espansione elettron.1(pls)"},
//{0x30,5,151,2,-1,"Valvola di espansione elettron.2(pls)"},
//{0x30,7,151,2,-1,"Valvola di espansione elettron.3(pls)"},
//{0x30,9,151,2,-1,"Valvola di espansione elettron.4(pls)"},
//{0x30,11,307,1,-1,"Valvola a 4 vie"},
//{0x30,12,307,1,-1,"Riscaldatore del carter"},
//{0x30,13,307,1,-1,"Y1S"},
//{0x30,13,306,1,-1,"SV (drain pan heater)"},
//{0x30,13,305,1,-1,"Y3S"},
//{0x00,0,998,1,-1,"In-Out separator"},
//{0xA0,0,119,2,1,"Temperatura linea di aspirazione"},
//{0xA0,2,119,2,1,"Temp. scambiatore di calore esterno"},
//{0xA0,4,119,2,1,"Temp. tubo del liquido"},
//{0xA0,6,119,2,2,"Pressione"},
//{0xA0,8,151,2,-1,"Valvola di espansione elettron.3(pls)"},
//{0xA0,10,152,1,-1,"O/U MPU ID"},
//{0xA0,11,152,1,-1,"O/U MPU ID"},
//{0xA0,12,307,1,-1,"HPS attivato"},
//{0xA0,12,306,1,-1,"Operazione di salvaguardia"},
//{0xA0,12,305,1,-1,"Riscaldatore del carter"},
//{0xA0,12,304,1,-1,"Elettrovalvola 3"},
//{0xA0,12,303,1,-1,"SV (drain pan heater)"},
//{0xA0,12,302,1,-1,"Elettrovalvola 1"},
//{0xA0,12,301,1,-1,"Valvola a 4 vie (Y1S)"},
//{0xA0,12,300,1,-1,"52C Output"},
//{0xA0,13,303,1,-1,"Calo temp. scarico"},
//{0xA0,13,302,1,-1,"Durante le operazioni di emergenza"},
//{0xA0,13,301,1,-1,"Bandiera unità interna da 50 ° C"},
//{0xA0,13,300,1,-1,"Bit potente (bit di impostazione MT)"},
//{0xA0,14,105,2,1,"Temperatura della porta del compressore"},
//{0xA1,0,119,2,1,"(Dati grezzi)Temperatura di ingresso scambiatore di calore acqua"},
//{0xA1,2,119,2,1,"(Dati grezzi)Temperatura di uscita dello scambiatore di calore dell acqua"},
//{0xA1,4,302,1,-1,"Elettrovalvola INJ liquida (Y4S)"},
//{0xA1,4,301,1,-1,"Riscaldatore piastra fondo"},
//{0xA1,4,300,1,-1,"Riscaldatore PHE"},
//{0xA1,5,114,2,1,"Temp. scarico target"},
//{0xA1,7,114,2,1,"Temperatura della porta target"},
//{0xA1,9,305,1,-1,"Impostazione Monobloc"},
//{0xA1,9,304,1,-1,"Impostazione Minichiller"},
//{0xA1,9,303,1,-1,"Impostazione MT"},
//{0xA1,9,302,1,-1,"Impostazione GSHP"},
//{0xA1,9,301,1,-1,"Impostazione Hydro split"},
//{0xA1,9,300,1,-1,"Impostazione Altherma LT"},
//{0x60,0,304,1,-1,"Abilita/disabilita dati"},
//{0x60,1,152,1,-1,"Indirizzo unità interna"},
//{0x60,2,315,1,-1,"I/U modo di funzionamento"},
//{0x60,2,303,1,-1,"Termostato ON/OFF"},
//{0x60,2,302,1,-1,"Prevenzione congelamento"},
//{0x60,2,301,1,-1,"Modalità silenziosa"},
//{0x60,2,300,1,-1,"Prevenzione del congelamento per le tubazioni dell acqua"},
//{0x60,3,204,1,-1,"Codice malfunzionamento"},
//{0x60,4,152,1,-1,"Codice dettagliato errore"},
//{0x60,5,203,1,-1,"Tipo malfunzionamento"},
//{0x60,6,219,1,-1,"Codice capacità I/U"},
//{0x60,7,105,2,1,"Valore di riferimento DHW"},
//{0x60,9,105,2,1,"Valore di riferimento LW (principale)"},
//{0x60,11,307,1,-1,"Interruttore portata d acqua"},
//{0x60,11,306,1,-1,"Protezione termica (Q1L) BUH"},
//{0x60,11,305,1,-1,"Protezione termica BSH"},
//{0x60,11,304,1,-1,"Alimentazione flusso kWh benefit"},
//{0x60,11,303,1,-1,"Entrata solare"},
//{0x60,11,302,1,-1,"Not translated yet"},
//{0x60,11,301,1,-1,"Not translated yet"},
//{0x60,11,300,1,-1,"Funzionamento bivalente"},
//{0x60,12,307,1,-1,"Valvola a 2 vie (On:Riscaldamento_Off:Raffreddamento)"},
//{0x60,12,306,1,-1,"Valvola a 3 vie (On:DHW_Off:Ambienti)"},
//{0x60,12,305,1,-1,"BSH"},
//{0x60,12,304,1,-1,"Gradino1 BUH"},
//{0x60,12,303,1,-1,"Gradino2 BUH"},
//{0x60,12,302,1,-1,"Valvola chiusura pavimento"},
//{0x60,12,301,1,-1,"Funzionamento pompa dell acqua"},
//{0x60,12,300,1,-1,"Funzionamento pompa solare"},
//{0x60,13,152,1,-1,"Codice opzione interna"},
//{0x60,15,215,1,-1,"I/U Software ID (xx)"},
//{0x60,14,215,1,-1,"I/U Software ID (yy)"},
//{0x60,16,152,1,-1,"I/U EEPROM Ver."},
//{0x61,0,307,1,-1,"Abilita/disabilita dati"},
//{0x61,1,152,1,-1,"Indirizzo unità interna"},
//{0x61,2,105,2,1,"Temp. acqua uscita prima BUH (R1T)"},
//{0x61,4,105,2,1,"Temp. acqua uscita dopo BUH (R2T)"},
//{0x61,6,105,2,1,"Lato liquido temp. refrig. (R3T)"},
//{0x61,8,105,2,1,"Temp. acqua ingresso (R4T)"},
//{0x61,10,105,2,1,"Temp. serbatoio DHW (R5T)"},
//{0x61,12,105,2,1,"Temp. ambiente interna (R1T)"},
//{0x61,14,105,2,1,"Sensore ambiente interno est. (R6T)"},
//{0x62,0,307,1,-1,"Abilita/disabilita dati"},
//{0x62,1,152,1,-1,"Indirizzo unità interna"},
//{0x62,2,307,1,-1,"Risc. prev/mant ON/OFF"},
//{0x62,2,306,1,-1,"Eco serbatoio ON/OFF"},
//{0x62,2,305,1,-1,"Comfort serbatoio ON/OFF"},
//{0x62,2,304,1,-1,"Funzionamento DHW potente. ON/OFF"},
//{0x62,2,303,1,-1,"Funzionamento riscaldamento di ambienti ON/OFF"},
//{0x62,2,302,1,-1,"Sistema OFF"},
//{0x62,2,301,1,-1,"Non in uso"},
//{0x62,2,300,1,-1,"Emergenza (interna) Attiva/non Attiva"},
//{0x62,3,105,2,1,"Valore di riferimento LW (aggiuntivo)"},
//{0x62,5,105,2,1,"Valore di riferimento RT"},
//{0x62,7,307,1,-1,"Raffredd. entrata RT est. agg."},
//{0x62,7,306,1,-1,"Riscaldam. entrata RT est. agg."},
//{0x62,7,305,1,-1,"Raffreddamento RT principale"},
//{0x62,7,304,1,-1,"Riscaldamento RT principale"},
//{0x62,7,303,1,-1,"Limite 4 consumo energetico"},
//{0x62,7,302,1,-1,"Limite 3 consumo energetico"},
//{0x62,7,301,1,-1,"Limite 2 consumo energetico"},
//{0x62,7,300,1,-1,"Limite 1 consumo energetico"},
//{0x62,8,307,1,-1,"Nessuno"},
//{0x62,8,306,1,-1,"Non in uso"},
//{0x62,8,305,1,-1,"Non in uso"},
//{0x62,8,304,1,-1,"Riscaldatore PHE"},
//{0x62,8,303,1,-1,"Prerisc. serbatoio ON/OFF"},
//{0x62,8,302,1,-1,"Funzionamento pompa di circolazione"},
//{0x62,8,301,1,-1,"Uscita allarme"},
//{0x62,8,300,1,-1,"Uscita funzionamento riscald. ambienti"},
//{0x62,9,105,2,-1,"Sensore flusso (l/min)"},
//{0x62,11,105,1,2,"Pressione acqua"},
//{0x62,12,152,1,-1,"Segnale pompa dell acqua (0:max-100:stop)"},
//{0x62,13,152,1,-1,"[Futuro] Mixing 1 valvola a 3 vie"},
//{0x62,14,152,1,-1,"[Futuro] Mixing 2 valvola a 3 vie"},
//{0x62,15,105,2,2,"Sensore di pressione del refrigerante"},
//{0x62,15,405,2,1,"Sensore di pressione(T)"},
//{0x63,0,307,1,-1,"Abilita/disabilita dati"},
//{0x63,1,152,1,-1,"Indirizzo unità interna"},
//{0x63,2,215,1,-1,"I/U EEPROM (3º cifra)"},
//{0x63,3,215,1,-1,"I/U EEPROM (4º 5º cifra)"},
//{0x63,4,215,1,-1,"I/U EEPROM (6º 7º cifra)"},
//{0x63,5,215,1,-1,"I/U EEPROM (8º 9º cifra)"},
//{0x63,6,215,1,-1,"I/U EEPROM (11º cifra)"},
//{0x63,7,215,1,-1,"I/U EEPROM (12º cifre)(rev.)"},
//{0x63,8,215,1,-1,"Non in uso"},
//{0x63,9,215,1,-1,"Non in uso"},
//{0x63,10,215,1,-1,"Non in uso"},
//{0x63,11,301,1,-1,"Not translated yet"},
//{0x63,12,300,1,-1,"Not translated yet"},
//{0x63,13,311,1,-1,"BUH capacità di output"},
//{0x63,14,161,1,-1,"Misura del sensore CT (L1)"},
//{0x63,15,161,1,-1,"Misura del sensore CT (L2)"},
//{0x63,16,307,1,-1,"HP Forced FG"},
//{0x63,16,161,1,-1,"Misura del sensore CT (L3)"},
//{0x64,0,307,1,-1,"Abilita/disabilita dati"},
//{0x64,1,152,1,-1,"Indirizzo unità interna"},
//{0x64,2,316,1,-1,"Modalità funz. ibrido"},
//{0x64,2,303,1,-1,"Funzionamento boiler su richiesta"},
//{0x64,2,302,1,-1,"Boiler DHW su richiesta"},
//{0x64,2,301,1,-1,"Uscita valvola bypass"},
//{0x64,3,105,2,-1,"BE_COP"},
//{0x64,5,105,2,1,"Temp. target riscaldamento ibrido"},
//{0x64,7,105,2,1,"Temp. target riscaldamento boiler"},
//{0x64,9,302,1,-1,"Add pump"},
//{0x64,9,301,1,-1,"Main pump"},
//{0x64,10,118,2,1,"Temperatura dell acqua miscelata"},
//{0x64,12,105,2,1,"2nd Domestic hot water temperature"},
//{0x64,14,152,1,1,"Target delta T heating"},
//{0x64,15,152,1,1,"Target delta T cooling"},
};
