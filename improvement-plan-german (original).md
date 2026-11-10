# ESPAltherma 2 – Review und Verbesserungsplan

Stand: Branch `beta`, Commit `505b7ab` (identisch mit `raomin/ESPAltherma:beta`), Review vom 10.11.2026.

## 1. Grundlage und Vorgehen

**Gelesen (vollständig):** `src/main.cpp`, `include/control.h`, `comm.h`, `config.h`, `converters.h`, `mqtt.h`,
`mqttserial.h`, `topics.h`, `netmgr.h`, `netpolicy.h`, `webserver.h`, `stats.h`, `catalog.h`, `survey.h`,
`restart.h`, `watchdog.h`, `stage.h`, `eventlog.h`, `eventring.h`, `logbuf.h`, `labeldef.h`, `board.h`,
`fwupdate.h`, `haremoval.h`, `homeassistant.h`, `power.h`, `src/homeassistant.cpp`, `web/index.html`,
`scripts/gen_catalog.py`, `scripts/build_web.py`, `telemetry/functions/api/*.js`, `telemetry/schema.sql`,
`platformio.ini`, `.github/workflows/build.yml`.

**Nur überflogen:** `detect.h`, `layoutfix.h`, `diii_comm.h`, `improv.h`, die Tests, die Doku.
**Nicht gelesen:** `flasher/`, `contrib/`, `scripts/webui_mock.py`, `scripts/check_i18n.py`, `release.yml`.

**Nicht gemacht:** Ich habe weder die Firmware gebaut noch die nativen Tests laufen lassen. Befunde mit dem
Vermerk „aus dem Code abgeleitet“ sind nicht auf einem Gerät reproduziert.

**Zusätzliche Quellen:**

- Claude-Session `2e24093b` (Inbetriebnahme am EHBX08EF6V + ERGA06EVH, Hydro-Software 20002203, PRs #643–#656,
  die Vorfälle vom 8. und 9.11.).
- Das laufende Gerät (ESP32-S3, 192.168.2.234), nur lesend: `/api/status`, `/api/events`, `/api/config`,
  `/api/detect`, `/api/values`.
- Die RAM-Abzüge unter `cl_ha/espaltherma2/` (`ram_dump_1.hex`, `ram_voll_2026-11-08.hex`,
  `ram_voll_2026-11-09.hex`). Ein neuer Abzug war nicht nötig: die drei vorhandenen decken alle Adressen ab,
  die `control.h` für 20002203 benutzt.
- Drei kleine Prüfskripte (Katalog, Topics/Discovery-Puffer/Profile, RAM-Adressen). Sie liegen nicht im Repo;
  Abschnitt 8 schlägt vor, ihre Prüfungen als Tests zu übernehmen.

**Legende:** *belegt* = am Gerät, im Dump oder per Skript nachgewiesen. *abgeleitet* = folgt aus dem Code, nicht
reproduziert. Schwere: hoch / mittel / niedrig.

## 2. Das Wichtigste in Kürze

1. **„Nur RAM“ schont den Speicher der Wärmepumpe, schreibt dafür bei jeder Zieländerung den Flash des ESP**
   (ganze Konfiguration plus Ereignisverlauf) und verdrängt den Ereignisverlauf. Am Gerät belegt (B1).
2. **Der Main-Loop steht 6–8 s „in publish values“**, weil unter der Werte-Sperre über das Netz gesendet wird;
   so lange wartet auch die Wärmepumpen-Task. Am Gerät belegt (B2).
3. **Lange Base-Topics machen MQTT kaputt:** ab 25 Zeichen abgeschnittene Topics und ungültiges Discovery-JSON,
   ab 30 Zeichen fallen Befehls- und Status-Topic des Leistungslimits zusammen. Per Skript belegt (B3).
4. **Laufzeitzustand steckt in der Konfiguration:** gehaltenes Ziel, Originalkurve und Limit werden mit dem
   Backup exportiert und beim Wiederherstellen zurückgeschrieben; parallele Änderungen der Wärmepumpen-Task
   können verloren gehen (B4).
5. **„Kurve wiederherstellen“ hat keine Rückfrage** und sitzt direkt neben „Apply“ – das passt zum Vorfall vom
   8.11., 15:43 (B6).
6. **Keine Absicherung der Schreib-Endpunkte gegen fremde Webseiten (CSRF)** und ein Setup-WLAN mit bekanntem
   Passwort, das nach 2 Minuten ohne Netz von selbst aufgeht (S1, S2).

Was gut ist und so bleiben sollte: die reine Logik (Regelung, Erkennung, Statistik, Netz-Policy) ist ohne
Arduino testbar und getestet, Handler der Weboberfläche ändern den Zustand nicht direkt, Schreibzugriffe auf
die Wärmepumpe laufen nur über eine Task, es gibt Watchdog, Ereignisverlauf und Firmware-Rollback. Die
Profiltabelle in `control.h` ist in sich konsistent (alle 15 Softwarestände erfüllen dieselben Abstände,
per Skript geprüft).

## 3. Bugs

### B1 – „Nur RAM“: Flash-Verschleiß am ESP und gefluteter Ereignisverlauf (hoch, belegt)

- **Wo:** `src/main.cpp` `controlTick()`: `c.heldChanged` → `configSave()` (Z. 855–862) und
  `eventAddf("%s target set to %d")` (Z. 841–842); `include/eventlog.h` `eventLoop()` → `eventSave()`.
- **Was passiert:** Jedes neue Ziel im „Nur RAM“-Betrieb setzt `held`, das wird in `config.ctlHeld` gespiegelt
  und die **ganze** Konfiguration als JSON in den NVS geschrieben (am Gerät 2338 Bytes ohne Passwörter).
  Zusätzlich landet jede Zieländerung im Ereignisverlauf, der spätestens alle 2 Minuten als Blob von
  ca. 2,3 KB gesichert wird. Der NVS ist nach der Partitionstabelle
  `min_spiffs.csv` des Arduino-Cores 20 KB groß (aus dem Gedächtnis, lokal nicht nachgeprüft).
- **Beleg:** Im Verlauf des Geräts sind 9 der 24 Einträge „Leaving water target set to N“ aus gut vier
  Stunden; Boot, Netz- und Update-Ereignisse sind schon fast herausgeschoben.
- **Auswirkung:** Die Option wird ausdrücklich für häufig wechselnde Ziele beworben (Automation nach Preis
  oder PV-Überschuss). Genau dann wird der ESP-Flash zum Verschleißteil: bei einer Änderung alle 5 Minuten
  grob einige Jahre, bei einer pro Minute deutlich unter einem Jahr bis zur Nenn-Lebensdauer der NVS-Seiten
  (Abschätzung, nicht gemessen). Der Ereignisverlauf verliert seinen Zweck als Postmortem.
- **Fix:**
  1. Gehaltenes Ziel und Limit-Zustand aus der Konfiguration lösen (siehe B4) und in einem eigenen, wenige
     Bytes großen NVS-Schlüssel ablegen; erst schreiben, wenn der Wert z. B. 10 Minuten stabil ist. Sofort
     nur im RTC-Speicher halten (überlebt Neustarts des ESP).
  2. Zieländerungen im Ereignisverlauf zusammenfassen („Leaving water target 24 → 30, 9 Änderungen“) oder
     ihnen das Token-Budget geben, das `netHistory()` schon hat.

### B2 – Netzwerk-Schreiben unter der Werte-Sperre: Main-Loop und Wärmepumpen-Task stehen (hoch, belegt)

- **Wo:** `src/main.cpp` `publishValues()` (Z. 788–802) → `updateValues()` → `mqttPublish()` je Wert;
  `include/mqtt.h` `publishHomeAssistantDeviceDiscovery()` (ganzer Discovery-Stream unter `valuesLock()`);
  `include/converters.h` `convert()` (Serial-Ausgabe je Wert, aufgerufen unter der Sperre).
- **Beleg:** Ereignisverlauf des Geräts: „Main loop stalled 8 s (in publish values)“ (10.11., 02:42) und
  „… 6 s (in publish values)“ (9.11., 20:25). Das Gerät läuft mit „One topic per value“, also 93 Publishes
  je Zyklus unter der Sperre.
- **Auswirkung:** Hängt ein Schreibzugriff (WLAN-Schluckauf), wartet `pollRegistries()` an `valuesLock()`.
  X10A-Abfragen, Regelungs-Ticks und beim HT-Außengerät die zeitkritischen DIII-Fenster verschieben sich,
  MQTT-Keepalive und Relais-Befehle ebenfalls.
- **Fix:** Unter der Sperre nur kopieren (Schnappschuss der `asString`-Werte bzw. der Label-Liste), danach
  ohne Sperre senden. Die Serial-Spur „Converting from: … -> …“ je Wert entfernen oder hinter einen
  Debug-Schalter legen; bei 115200 Baud sind das pro Zyklus mehrere hundert Millisekunden unter der Sperre.

### B3 – Base-Topic: abgeschnittene Topics, ungültiges Discovery-JSON, Befehls-Echo (hoch, belegt)

- **Wo:** `include/topics.h` (Topic-Puffer `char[48]`, Base bis 32 Zeichen); `include/mqtt.h`
  `publishControl()` (`json[640]`), `publishLimit()` (`json[640]`), `mqttConnectOnce()` (`json[1100]`).
- **Beleg (Skript):**
  - ab 25 Zeichen Base: `…/power_limit/attributes` abgeschnitten, ab 26 auch die beiden
    `…/attributes` der Ziele;
  - ab ca. 25 Zeichen Base: Discovery-JSON der Ziel-Entität größer als 640 Bytes (515 beim Standard,
    633 bei 24, 662 bei 28, 685 bei 32 Zeichen) → `snprintf` schneidet ab, das JSON ist ungültig, die
    Entität erscheint nie – ohne Meldung;
  - bei 30–32 Zeichen Base: `power_limit/on/set` und `power_limit/on/state` sind nach dem Abschneiden
    **identisch**.
- **Auswirkung des letzten Punkts (abgeleitet):** Das Board abonniert das Topic, auf dem es seinen Zustand
  retained veröffentlicht. „ON“ kommt als Befehl zurück → `requestOn(true)` → `apply()` schreibt ins RAM der
  Wärmepumpe → Zustand wird wieder veröffentlicht → Endlosschleife von RAM-Schreibzugriffen.
- **Fix:** Puffer aus der maximalen Base-Länge ableiten (oder Base auf 24 Zeichen begrenzen und das in der
  GUI sagen), Discovery-JSON nicht mehr in feste Puffer drucken (siehe A8), Test „längste Base: nichts
  abgeschnitten, alle Topics verschieden“ in `test_topics` (dort fehlt jede Längenprüfung).

### B4 – Laufzeitzustand in der Konfiguration: Backup-Restore und verlorene Änderungen (hoch, belegt/abgeleitet)

- **Wo:** `include/config.h` (`ctlOrig`, `ctlHeld`, `ctlLimitOn/Value/Orig/Sw`, `telemetrySent`,
  `detectedKey`, `installId` in `configToJson`/`configFromJson`); `include/webserver.h` POST `/api/config`
  (Z. 571–594) und `webLoop()` (`config = *next`, Z. 1189); `web/index.html` `cfgExport`/`cfgImport`
  (Z. 1447–1455).
- **Beleg:** `/api/config` des Geräts enthält `held_lwt: 30`, `orig_lwt_hi/lo: 25/38`, `orig_sw`,
  `limit_*`, `telemetry.sent`, `install_id`. „Download a backup“ speichert genau diese Antwort, „Restore a
  backup“ schickt die Datei unverändert und ohne Rückfrage zurück.
- **Auswirkungen (abgeleitet):**
  - Ein älteres Backup bringt ein altes gehaltenes Ziel oder ein altes Leistungslimit zurück; nach dem
    nächsten Neustart schreibt die Firmware es in die Wärmepumpe.
  - Eine veraltete „Originalkurve“ aus dem Backup würde bei „Kurve wiederherstellen“ in den **Speicher** der
    Wärmepumpe geschrieben.
  - Dieselbe `install_id` auf zwei Boards, wenn ein Backup auf ein zweites Board gespielt wird.
  - Verlorene Änderung: Der Handler kopiert `config` in der Web-Task, `webLoop()` übernimmt die Kopie später.
    Was die Wärmepumpen-Task dazwischen ändert und speichert (gehaltenes Ziel, Originalkurve, erkanntes
    Modell, Layout-Korrekturen), wird überschrieben. Das Fenster ist normalerweise kurz, wächst aber mit
    jedem Stillstand des Main-Loops (B2: 6–8 s).
  - `configSave()` wird aus drei Tasks ohne Sperre aufgerufen und serialisiert die Struktur, während eine
    andere Task sie ändert.
- **Fix:** Einstellungen und Laufzeitzustand trennen (eigene Struktur, eigener NVS-Schlüssel, nicht im
  Export, vom API nicht annehmbar). Übernahme feldweise statt als ganze Struktur, eine Sperre um Lesen,
  Ändern und Speichern. Beim Restore: Rückfrage, Prüfung auf ein Versionsfeld, Hinweis „Passwörter danach
  neu eintragen“.

### B5 – Leistungslimit: kein früher Wiederholversuch, keine Plausibilitätsprüfung vor dem Schreiben (mittel, abgeleitet)

- **Wo:** `include/control.h` `LimitControl::tick()`/`identify()`/`readBack()`/`apply()`.
- **Was fehlt gegenüber `CurveControl`:**
  1. Der gestaffelte Wiederholversuch aus #643 (30 s, dann verdoppelt). `LimitControl` versucht es nach
     einem Fehlschlag erst nach 10 Minuten wieder. Nach einem gemeinsamen Einschalten (ESP am X10A
     versorgt) wird ein gespeichertes Limit also bis zu 10 Minuten zu spät wieder gesetzt.
  2. Eine Prüfung, ob die gelesenen Werte Sinn ergeben. `kind` wird einmal gelesen (`1` = kW, alles andere
     = A). Antwortet die Platine, bevor ihr RAM geladen ist (laut Kommentaren in `control.h` liest sie dann
     Nullen), wird ein kW-Limit als Ampere behandelt: `restore()` → `apply()` schreibt das Ampere-Byte und
     setzt das Enable-Byte auf 1, mit einem Zahlenwert, der als kW gemeint war. `kind` bleibt bis zur
     nächsten Identifikation (10 min) falsch; der Pfad „erneut anwenden“ ruft `apply(value)` ohne `clamp()`.
  3. Bei den per Software-Analyse gemappten, „noch auf keinem Gerät bestätigten“ Profilen gibt es damit für
     das Limit gar keine Schutzprüfung vor dem ersten Schreibzugriff.
- **Beleg für sinnvolle Grenzen:** Alle drei Abzüge der 20002203 zeigen Enable = 2, Art = 1, Limit 1 =
  20,0 kW, 50 A, aktives Limit 20,0 kW / 50,0 A.
- **Fix:** `READY` nur, wenn Enable ∈ {0, 1, 2}, Art ∈ {0, 1} und Limit 1 im physikalischen Bereich liegt
  (z. B. 1–25 kW bzw. 4–75 A, kein NaN); sonst „noch nicht bereit“ mit demselben Backoff wie bei den Zielen.
  Art bei jedem `readBack()` mitlesen. Testfall „Antwort vor geladenem RAM“ in `test_control`.
- **Zusatz für die Ziele:** Die Bereichs-Datensätze haben in allen Abzügen dieselbe Form
  (`ff 00 <Spanne> <Minimum> 08`). Byte 0, 1 und 4 mitzuprüfen wäre eine billige zweite Signatur vor dem
  ersten Schreiben auf einem unbestätigten Softwarestand.

### B6 – „Kurve wiederherstellen“: keine Rückfrage, falsche Auswertung, keine Herkunft (mittel, belegt/abgeleitet)

- **Wo:** `web/index.html` Z. 867–870 (Button ohne `confirm`, in derselben Zeile direkt nach „Apply“);
  `include/webserver.h` Z. 813 (`doc["lwt_restore"].is<bool>()`: auch `false` löst aus);
  `src/main.cpp` Z. 836–843 (Ereignis ohne Auslöser).
- **Vorfall:** Am 8.11., 15:43 wurde die Kurve wiederhergestellt, das gehaltene Ziel fiel weg, die
  Wärmepumpe pendelte bis 16:30. Weder Home Assistant noch die damalige Session hatten es ausgelöst; im
  Verlauf stand nur „curve restored“.
- **Fix:** Rückfrage mit den Folgen im Klartext („Ziel 28 °C wird aufgegeben, die Wärmepumpe folgt wieder
  ihrer Kurve 25–38 °C“), Button optisch abgesetzt. Im API nur `true` annehmen. Jeden Schreibbefehl mit
  Herkunft ins Ereignis schreiben („web 192.168.2.x“ / „MQTT“).
- Ebenfalls ohne Rückfrage: „Restart“ (löst die Relais), „Undo the corrections“, „Restore a backup“, und das
  Speichern geänderter Pins (startet sofort neu).

### B7 – Die Startseite schreibt beim bloßen Ansehen in die Konfiguration (mittel, abgeleitet)

- **Wo:** `web/index.html` `renderToday()` Z. 713–715: `post('/api/config', {stats: {tz}})`, sobald der
  UTC-Versatz des Browsers von der gespeicherten Zeitzone abweicht.
- **Auswirkung:** Nach jeder Zeitumstellung, vom Handy im Ausland oder bei zwei Betrachtern in
  verschiedenen Zeitzonen wird die Konfiguration geschrieben. Ist ein Admin-Passwort gesetzt (am Gerät der
  Fall), löst dieser POST die Passwortabfrage des Browsers auf der Startseite aus – genau das, was der
  Kommentar am öffentlichen `GET /api/config` vermeiden wollte. Bis jemand die Seite öffnet, liegt der
  Tageswechsel der Statistik nach einer Zeitumstellung eine Stunde daneben.
- **Fix:** Einmalig eine Zeitzonen-Regel speichern (aus `Intl.DateTimeFormat().resolvedOptions().timeZone`,
  als POSIX-TZ) und `configTzTime()` benutzen; kein Schreiben beim Rendern.

### B8 – Smart Grid mit nur einem Pin: Entität da, nichts schaltet (mittel, abgeleitet)

- **Wo:** `src/main.cpp` `setupPins()` richtet die Ausgänge nur ein, wenn **beide** Pins gesetzt sind;
  `include/mqtt.h` (Discovery, `callback()`, `callbackSg()`) und `statusJson()` prüfen nur `sg1Pin >= 0`.
- **Auswirkung:** Mit nur SG1 erscheint die Auswahl in Home Assistant, der Modus wird bestätigt, aber kein
  Ausgang ist konfiguriert; `digitalWrite()` läuft auf Pin −1.
- **Fix:** Eine Prädikatsfunktion `smartGridConfigured()` (beide Pins) überall verwenden; beim Speichern
  ablehnen, wenn nur einer gesetzt ist.

### B9 – Thermostat-Relais beim Start nicht in den Ruhepegel gesetzt (niedrig bis mittel, abgeleitet)

- **Wo:** `src/main.cpp` `setupPins()`: `pinMode(config.thermPin, OUTPUT)` ohne vorheriges `digitalWrite`
  (Safety und Smart Grid machen es richtig); der gespeicherte Zustand kommt erst in `readEEPROM()` nach
  `setupScreen()` (2 s Begrüßungsbild auf den M5-Boards).
- **Auswirkung:** Ein low-aktives Thermostat-Relais ist beim Start kurz angezogen, auf M5-Boards rund
  2 Sekunden. Zusammen mit R4 (Neustart alle 15 Minuten bei fehlendem Broker) wird daraus ein
  wiederkehrender Impuls.
- **Fix:** Erst den wiederherzustellenden Pegel schreiben, dann `pinMode`.

### B10 – Werte-JSON kann still verschwinden (niedrig, belegt als Rechnung)

- **Wo:** `include/mqtt.h` `mqttPublish()` (liefert bei „größer als der Puffer“ kommentarlos `false`),
  `sendValues()`, `src/main.cpp` `updateValues()` (`snprintf` schneidet ab, danach wird das letzte Zeichen
  blind durch `}` ersetzt; `strcat(jsonbuff, "]")` kann im Tabellenformat ein Byte überlaufen).
- **Rechnung:** 128 Werte mit den längsten Namen eines ECH2O-Modells ergeben etwa 5,5 KB, dazu Statistik
  (ca. 0,8 KB) und Zähler der Einheit (ca. 0,9 KB) – mehr als die nutzbaren 7,1 KB. Bei üblichen Auswahlen
  (am Gerät 93 Werte) ist Platz.
- **Fix:** Länge mitführen statt `strlen` je Wert, bei Überlauf Log-Zeile und Ereignis, Warnung in der
  Werte-Seite; besser in mehreren Nachrichten oder gestreamt senden wie die Discovery.

### B11 – Doppelte Namen innerhalb eines Registers (niedrig, belegt)

- „O/U MPU ID“ steht in 17 Definitionen auf 0xA0/10 **und** 0xA0/11; „Main pump“ und „Add pump“ in den
  beiden Multi-DHW-Tank-Definitionen auf 0x64/2 und 0x64/9. Werden beide gewählt, teilen sie MQTT-Schlüssel
  und Home-Assistant-ID; einer verdeckt den anderen.
- `scripts/gen_catalog.py` prüft nur „derselbe Name in mehreren Registern“. Fix: Prüfung auf „ein Name, ein
  Platz“ erweitern und die drei Namen eindeutig machen.

### B12 – Kleinere Fehler

| Nr. | Wo | Fehler | Fix |
|---|---|---|---|
| a | `converters.h` Fall 211 | `if (data == 0)` prüft den Zeiger, nicht das Byte: „OFF“ kommt nie | `data[0] == 0` |
| b | `converters.h` Ende von `convert()` | `dblData != NAN` ist immer wahr | `!isnan(dblData)` |
| c | `converters.h` Fall 100 | `strlcat(…, num)` mit der Datenlänge als Puffergröße; im Katalog unbenutzt | Fall entfernen oder korrigieren |
| d | `converters.h` Z. 3 | ungenutztes globales `char buff[64]` | entfernen |
| e | `webserver.h` `webLoop()` | `debug_serial` fehlt in `mqttChanged`: das Abo greift erst nach der nächsten Neuverbindung; geänderter Zeitserver wirkt erst nach einem Neustart, ohne Hinweis | in die Änderungsprüfung aufnehmen bzw. Neustart melden |
| f | `main.cpp` `statsSetup()` | die Wärme-Obergrenze aus der Gerätekapazität wird nur beim Start und bei geänderten Statistik-Einstellungen gesetzt, nicht nach der ersten Erkennung | bei Modellwechsel erneut aufrufen |
| g | `stats.h` `resetDay()` | löscht um Mitternacht den Ring der Startzeiten: „Starts letzte Stunde“ fällt auf 0 | Ring über den Tageswechsel behalten |
| h | `mqtt.h` SG-Discovery | Gerätemodell fest „M5StickC PLUS ESP32-PICO“; Namen uneinheitlich („EspAltherma Smart Grid“, „Altherma“, „AlthermaSensors“) | `BOARD_NAME`, einheitliche Namen |
| i | `homeassistant.cpp` `discoveryStart()` | `"sw":"0.1"` fest; kein `sw_version`, keine `configuration_url` | `ESPALTHERMA_VERSION`, Link auf die Weboberfläche |
| j | `comm.h` `queryRegistryUnlocked()` | `millis() < start + SER_TIMEOUT` (nicht überlaufsicher); eine genau zur Frist vollständige Antwort gilt als Timeout | Differenz-Idiom, nach Länge statt nach Zeit entscheiden |
| k | `main.cpp` `hpTask()` | `HP_STAGE("hot water target")` deckt beide Ziele, Limit, Zähler, Speicher-Lesen und DIII ab: die Watchdog-Meldung führt in die Irre | Stufe je Schritt |
| l | `main.cpp` `haExtraSetup()` | überschreibt die statische Liste, während der Main-Loop sie für die Discovery lesen kann; `haExtraCount = 0` kommt erst nach dem Überschreiben | zweite Liste füllen, dann umschalten, unter der Werte-Sperre |
| m | `main.cpp` | `busy` ist ein einfaches `bool`, wird aber aus zwei Tasks benutzt | `volatile`/atomar |
| n | `webserver.h` `collectBody()` | ein Body über 8 KB wird als „invalid JSON“ gemeldet | eigene Meldung (413) |
| o | `survey.h` / `main.cpp` | `surveyJson[3072]` wird bei Überlänge still abgeschnitten (ungültiges JSON für Weboberfläche, MQTT und Telemetrie); am Gerät 1093 Bytes, also weit weg | Abschneiden erkennen und melden |

## 4. Robustheit

### R1 – Pins ohne Prüfung, kein abgesicherter Start (hoch, abgeleitet)

`configFromJson()` und der Handler prüfen weder, ob ein Pin auf diesem Board existiert und als Ausgang
taugt (Flash-Pins, nur-Eingang, USB, Strapping), noch Kollisionen (RX = TX, Relais auf einem X10A-Pin).
Ein falscher Pin kann beim Start abstürzen; dann startet das Board in Schleife und ist nur per USB zu
retten. Der Rollback greift nur bei frisch geflashter Firmware. Auch `frequency` ist serverseitig
unbegrenzt (die GUI erzwingt 5 s nur im Browser; über 600 s integriert die Statistik nichts mehr).

- Je Board eine Tabelle der erlaubten GPIOs in `board.h`; serverseitig prüfen und mit Feldnamen ablehnen.
- Abgesicherter Start: Zähler im RTC-Speicher; nach drei Abstürzen kurz nach dem Start ohne X10A-UART und
  Relais starten, Setup-WLAN an, Banner „Pins prüfen“.

### R2 – Konfiguration: fehlgeschlagenes Speichern und kaputte Daten bleiben unbemerkt (mittel, abgeleitet)

`configSave()` prüft das Ergebnis von `putString()` nicht (NVS-Zeichenketten sind auf 4000 Bytes begrenzt;
128 Werte, vier Layout-Korrekturen und Passwörter kommen in die Nähe). `configLoad()` ersetzt eine nicht
lesbare Konfiguration stillschweigend durch die Werkseinstellungen und speichert diese.

- Ergebnis prüfen, Fehler in Log, Ereignis und API-Antwort melden.
- Zwei Schlüssel im Wechsel (letzter guter Stand), Versionsfeld, bei Lesefehler Ereignis statt
  stillem Überschreiben.

### R3 – Zähler der Einheit: ein Ausreißer bleibt hängen, ein Neustart lässt sie zurückspringen (mittel, belegt/abgeleitet)

- `MeterReader::readCounters()` behält je Zähler den höchsten je gelesenen Wert. Ein einziger zu hoher
  Lesewert (z. B. ein Lesezugriff mitten in einer Aktualisierung) bleibt bis zum nächsten ESP-Neustart
  stehen und landet dauerhaft in der Langzeitstatistik von Home Assistant.
- **Beleg für den umgekehrten Fall:** Nach dem Neustart der Wärmepumpe stehen die Wärme-Floats auf ganzen
  kWh (9.11.: 12 940,0 und 4 881,0; am 8.11. noch 12 940,573 und 4 881,828). Das gemerkte Maximum verdeckt
  das nur bis zum nächsten ESP-Neustart; danach sieht Home Assistant einen Rückschritt.
- Vorschlag: Anstieg erst übernehmen, wenn ein zweiter Lesevorgang ihn bestätigt und er zur verstrichenen
  Zeit passt; Maxima im RTC-Speicher halten; bei geänderter Software-ID zurücksetzen.

### R4 – Neustart-Politik bei fehlendem Broker (mittel, abgeleitet)

„MQTT broker unreachable for 15 minutes“ startet das Board neu – und danach alle 15 Minuten wieder,
solange der Broker fehlt (Wartung am Home-Assistant-Host). Die WLAN-Regel kennt „seit dem Start nie
online → 30 Minuten“, die MQTT-Regel nicht. Jeder Neustart setzt Smart Grid auf 0 und das Safety-Relais
auf inaktiv (beide werden nicht gespeichert, das Thermostat-Relais schon), löst B9 aus und startet alle
Identifikationen neu.

- Gestaffelt neu starten (15 min, 1 h, 6 h), höchstens einmal, solange die Weboberfläche erreichbar war.
- Bewusst entscheiden und dokumentieren, welche Relais einen Neustart überdauern; in der GUI anzeigen.
- MQTT-Verbindungsversuche mit Backoff (heute alle 5 s ein blockierender Versuch im Main-Loop).

### R5 – Home Assistant: veraltete Werte, verschwindende Entitäten (mittel, abgeleitet; ein Symptom belegt)

- Antwortet die Wärmepumpe nicht mehr oder fällt ein Register aus, fehlen die Schlüssel im JSON. Die
  Sensoren behalten ihren letzten Wert unbegrenzt, Home Assistant loggt je Zyklus Template-Fehler. Für eine
  Regelung in Home Assistant, die auf diesen Werten rechnet, ist das gefährlich. → `expire_after` (z. B.
  dreifaches Abfrageintervall) und ein Verbindungs-Sensor „Wärmepumpe antwortet“.
- Ist ein Ziel nicht bereit, wird seine Entität **gelöscht** (leere Discovery), nicht „nicht verfügbar“.
  Dashboards zeigen „Entität nicht gefunden“, der Grund (`not_curve_mode`, `unsupported`, `no_answer`) ist
  in Home Assistant nicht sichtbar. In der Session war das Vorlauf-Ziel nachts häufig kurz „unavailable“.
  → Entität behalten, zweites Availability-Topic je Regelung, Status als Attribut oder Diagnose-Sensor.
- Ein einzelner fehlgeschlagener Austausch setzt den Status sofort auf `no_answer`. → erst nach zwei bis
  drei Fehlversuchen in Folge.

### R6 – Halb geschriebene Kurve, verworfener Auftrag (niedrig, abgeleitet)

`CurveControl::apply()`: Gelingt Punkt A und scheitert Punkt B, steht eine halbe Kurve im RAM, der Auftrag
wird ohne Wiederholung und ohne Ereignis verworfen (nur der „Nur RAM“-Halt repariert es später). →
bis zu drei Versuche, sonst Ereignis „Schreiben fehlgeschlagen“. Außerdem kann ein Wiederherstellen im
„Nur RAM“-Betrieb `persist()` aufrufen, wenn der gespeicherte Stand nicht gelesen werden konnte – entgegen
„der Speicher wird nie geschrieben“.

### R7 – Nicht vorhandene Fühler sehen aus wie Messwerte (niedrig, belegt als Symptom)

`readRegistryValues()` prüft nicht, ob Offset + Größe innerhalb der tatsächlich empfangenen Antwort
liegen; dahinter liegende Werte dekodieren Nullen und werden als 0, −51,18 (Druck → Temperatur von 0) oder
−40,6 veröffentlicht. Am Gerät stehen 15 der 93 Werte auf 0; ob „nicht vorhanden“ oder „wirklich 0“, ist
nicht zu unterscheiden. → Länge prüfen (Protokoll I liefert sie) und „unbekannt“ senden; bekannte
Leerwerte je Umrechnung als „unbekannt“ behandeln.

### R8 – Weitere Punkte

- Blockierende Aufrufe im Main-Loop: MQTT-Verbindung, MQTT-Test, Broker-Suche, Telemetrie, Heartbeat,
  Update-Prüfung (TLS, 8 s Timeout). Entweder in eine eigene Netz-Task oder strikt nacheinander mit Budget.
- `heap_min` am Gerät 58 KB; die TLS-Aufrufe prüfen nur den aktuellen freien Heap (70 KB). Größten freien
  Block prüfen.
- Stack der Wärmepumpen-Task (8 KB) ohne Messung; `uxTaskGetStackHighWaterMark` in den Status aufnehmen.
- Ein Topic-Befehl, der mit „R“ beginnt, startet das Board neu; als retained Nachricht ergibt das eine
  Neustart-Schleife bei jeder Verbindung. Nur exakt „RESET“/„REBOOT“ annehmen, in den ersten Sekunden nach
  dem Verbinden ignorieren.
- Zustand in altem `EEPROM` (Thermostat) neben dem NVS: zusammenführen.
- Web-Update setzt `busy` nicht: Abfragen und Regelung laufen während des Flashens weiter.

## 5. Sicherheit

### S1 – Schreibende Endpunkte ohne Schutz vor fremden Webseiten (hoch, abgeleitet)

Die POST-Handler (`/api/config`, `/api/control`, `/api/reboot`, `/api/factory-reset`, `/update`) prüfen
weder `Content-Type` noch `Origin`/`Host`. Eine beliebige Webseite im Browser des Nutzers kann per
einfachem Formular- oder `fetch`-Aufruf Ziele setzen, den MQTT-Server umstellen (das Board meldet sich dann
mit Benutzer und Passwort beim fremden Broker an) oder eine Firmware hochladen. Ohne Admin-Passwort
uneingeschränkt; mit Passwort schickt der Browser die gemerkten Zugangsdaten mit. Dasselbe gilt für
DNS-Rebinding.

- Bei zustandsändernden Anfragen `Origin` gegen den eigenen Host prüfen und einen eigenen Header verlangen
  (den `post()` der Seite mitschickt); `Content-Type: application/json` erzwingen.
- Das Telemetrie-Backend macht genau diese Content-Type-Prüfung bereits.

### S2 – Setup-WLAN mit bekanntem Passwort geht von selbst auf (mittel, abgeleitet)

`AP_PASSWORD "espaltherma"`, Start nach 2 Minuten ohne Netz auch bei konfiguriertem WLAN (Router-Neustart).
Solange kein Admin-Passwort gesetzt ist, hat jeder in Funkreichweite die ganze Weboberfläche samt
Firmware-Upload und Schreibzugriff auf die Wärmepumpe. `ArduinoOTA` ist ohne Admin-Passwort ebenfalls offen.

- Im Einrichtungsassistenten ein Admin-Passwort aktiv anbieten.
- Option „Setup-WLAN nur ohne konfiguriertes Netz“; Passwort je Gerät (aus der MAC, auf Seriell/Display)
  oder einstellbar.

### S3 – Weitere Punkte (niedrig)

- `POST /api/discover` (blockiert den Main-Loop sekundenlang) und `GET /api/wifi/scan` (startet Scans)
  brauchen keine Anmeldung.
- „MQTT ↔ serial gateway“: jeder Client am Broker kann beliebige X10A-Frames senden, auch Speicher-
  Schreibbefehle. Die GUI nennt das Risiko nicht. Der Empfang hält die serielle Sperre, solange Bytes
  kommen (ohne Obergrenze), und ruft `client.loop()` rekursiv auf.
- TLS überall mit `setInsecure()` (MQTT, Telemetrie, Update-Prüfung). Für MQTT optional ein CA-Zertifikat.
- Firmware-Upload prüft nicht, ob das Image für dieses Board gebaut wurde (gleicher Chip, andere Pins).
  Projekt-/Board-Kennung im Image prüfen; die GUI kann den Dateinamen gegen `status.board` halten.
- Telemetrie: das Tageslimit ist global; wer es mit zufälligen IDs füllt, sperrt echte Berichte für den Tag.

## 6. Verständlichkeit der GUI

| Nr. | Beobachtung | Vorschlag |
|---|---|---|
| G1 | Rückfragen fehlen bei folgenreichen Aktionen (B6). | Einheitlicher Bestätigungsdialog mit Folgen im Klartext; gefährliche Buttons abgesetzt. |
| G2 | „Experimental“: drei dichte Absätze; die Voraussetzungen werden erst nach dem Speichern auf der Startseite sichtbar. In der Session: „Muss ich dazu vorher was in der WP umstellen?“ | Je Funktion eine Live-Checkliste direkt am Häkchen: Software-ID unterstützt, Sollwertmodus witterungsgeführt (mit Menüpfad am Bedienteil), erlaubter Bereich. Text auf zwei Sätze kürzen, Rest hinter „Mehr“. |
| G3 | Feldeinstellungen wie [9-00], [C-08], [4-08] stehen ohne Erklärung im Text. In der Session: „4-08 … wie kann ich das ablesen?“ | Kurzes Glossar bzw. Tooltip mit Namen und Menüpfad; wo die Firmware den Wert kennt, ihn anzeigen. |
| G4 | Pins sind nackte Zahlenfelder; „−1 = unbenutzt“ steht nur bei den Relais. In der Session führte die Board-Beschriftung (16/17 gegen GPIO17/18) zu vertauschten Leitungen. | Auswahlliste je Board mit Hinweisen (Strapping, USB, nur Eingang), Kollisionsprüfung, Button „RX/TX tauschen“ und „Verbindung testen“ direkt an der Karte; Neustart erst nach Rückfrage. |
| G5 | Einstellungen: ein langes Formular, ein Speichern-Button ganz unten, kein Hinweis auf ungespeicherte Änderungen; Tab- oder Sprachwechsel verwirft Eingaben. | Speichern je Karte oder „ungespeichert“-Markierung mit Warnung beim Verlassen. |
| G6 | Werte: gruppiert nach „Registry 0x61“; ein Routenwechsel lädt den Katalog neu und verwirft ungespeicherte Häkchen. | Gruppen nach Bedeutung (Temperaturen, Drücke, Zustände, Zähler), Registersicht als Umschalter; Auswahl beim Neuladen behalten. |
| G7 | Nichts zeigt, welche Werte das Schaubild, die Statistik und die Home-Assistant-Karten brauchen. Wer „Flow sensor“ abwählt, bekommt „—“ ohne Grund. | Markierung „wird für Startseite/Statistik gebraucht“, Warnung beim Abwählen; die „Today“-Karte nennt den fehlenden Eingang. |
| G8 | Erklärungen nur als `title`-Tooltip: auf dem Handy nicht erreichbar. | Antippbares Info-Symbol oder ausklappbarer Text. |
| G9 | Nach Firmware-Update oder Neustart bleibt die alte Seite stehen (altes Skript gegen neue Firmware). | Auf neue Build-Kennung warten und neu laden; Banner bei abweichender Kennung. |
| G10 | Firmware-Datei: jede `.bin` wird angenommen. | Dateiname gegen Board prüfen, „factory“-Image ablehnen, Hinweis „nicht schließen“. |
| G11 | Speicher lesen: höchstens 64 Bytes je Abruf. Wer beiträgt, schreibt eigene Skripte (Session: `speicher_lesen.py`; mehrere Abzüge in #599). | „Bereich als Datei laden“ mit Fortschritt; Format wie in der Doku. |
| G12 | Mit Admin-Passwort sieht ein nicht angemeldeter Besucher alle Bedienelemente; Fehler kommen erst beim Klick. | Lesemodus kenntlich machen, Anmelden-Button. |
| G13 | Werte abwesender Fühler (0, −51,18) erscheinen wie Messwerte (R7). | Als „—“ zeigen, sobald die Firmware „unbekannt“ liefert. |
| G14 | Kein Ort zeigt, was in Home Assistant ankommt (Gerätename, Base-Topic, Anzahl der Entitäten, Zustand der Ziel-Entitäten). | Kleine Karte „Home Assistant“ auf der Systemseite. |

## 7. Architektur

### A1 – Eine Übersetzungseinheit, globale Variablen, Reihenfolge der Includes

Fast alles liegt als Implementierung in Headern (`comm.h`, `mqtt.h`, `netmgr.h`, `webserver.h` definieren
Globale und nicht-inline-Funktionen) und funktioniert nur, weil `main.cpp` sie in der richtigen Reihenfolge
einbindet. Abhängigkeiten laufen über `extern` und wiederholte Vorwärtsdeklarationen (`valuesLock()` ist
viermal in drei Dateien deklariert). Vorschlag in Schritten: die Arduino-abhängigen Teile in `.cpp` mit schmalen Headern
überführen (zuerst `comm`, `mqtt`, `webserver`); die reinen Logik-Header bleiben, wie sie sind.

### A2 – `main.cpp` zerlegen

1200 Zeilen mit Bildschirm, Label-Aufbau, Survey-Ablauf, Layout-Korrektur, Statistik-Anbindung,
Home-Assistant-Zusatzwerten, Regelungs-Anbindung, Speicher-Diagnose, Task und Setup. Ziel: `setup()` und
`loop()` rufen wenige benannte Schritte auf; je Thema ein Modul (`hp_task`, `survey_run`, `stats_glue`,
`ha_extras`, `control_glue`, `memory_diag`, `screen`).

### A3 – Bedeutung über Rollen statt über Namensvergleiche

Welche Rolle ein Wert hat, wird an fünf Stellen aus dem englischen Namen geraten: `statsValue()`-Präfixe
(`main.cpp`), die `KEYS`-Regexe der Startseite, die Einheiten in `homeassistant.cpp` („l/min“, „(A)“,
„(rps)“), die Zwei-Zonen-Namen in `catalog.h`, die Größenarten in `detect.h`. Sie laufen schon auseinander:
„Leaving water temp. (hydrobox)“ gilt in der Firmware als „vor dem Heizstab“, in der Seite als „danach“.
Vorschlag: `gen_catalog.py` vergibt je Eintrag eine Rolle und eine Einheit (Tabelle im Skript, geprüft
beim Generieren); Statistik, Startseite (`/api/values` liefert die Rolle mit) und Discovery lesen sie.

### A4 – Ein Decoder

Rohbytes werden dreimal dekodiert: `Converter::convert()`, `detectDecode()`, `layoutDecode()`. Die linearen
Umrechnungen (101–118, 151–165) als Tabelle {vorzeichenbehaftet, Byte-Reihenfolge, Faktor} in einem
Arduino-freien Header, den alle drei benutzen; der 370-Zeilen-`switch` schrumpft auf die Sonderfälle.

### A5 – Konfiguration

Je Einstellung vier Stellen (Struktur, Standardwert, `toJson`, `fromJson`) plus GUI-Feld, Übersetzung und
die handgeschriebenen „hat sich geändert“-Vergleiche in `webLoop()` (dort fehlt bereits ein Feld, B12 e).
Vorschlag: Feldtabelle (Name, Typ, Standard, Grenzen, „braucht Neustart“, betroffene Gruppe) als einzige
Quelle; daraus Laden, Speichern, Validieren und die Änderungsgruppen. Dazu Versionsfeld mit Migrationen
statt Heuristiken bei jedem Laden, Trennung vom Laufzeitzustand (B4) und eine Sperre.

### A6 – Identität der Einheit und Profile

Vier Bausteine (Warmwasser, Vorlauf, Limit, Zähler) lesen jeweils selbst die Software-ID und suchen ihr
Profil (die Suchschleife steht dreimal im Code). Vorschlag: eine `UnitIdentity` (Software-ID, Profil,
Modellname, „RAM geladen“), die einmal identifiziert und von allen benutzt wird – das löst auch B5
einheitlich. Die Profile mit benannten Feldern statt Positionslisten aus 13 Hex-Zahlen; die festen
Abstände (B − A = 0x3E, Vorlauf = Warmwasser + 2, Art = Enable + 1, Zähler = Strom gesamt + 4 …) als Test.

### A7 – Rückmeldungen der Regelung

`control.h` meldet über Flags, die `main.cpp` abfragt und zurücksetzt (`changed`, `heldChanged`,
`origChanged`, `applied`, `released`, `stateChanged`). Eine kleine Ereignisstruktur („Ziel gesetzt“,
„Kurve zurück“, „Halt geändert“, „Schreiben fehlgeschlagen“) mit Herkunft macht das Protokoll sichtbar und
liefert die Herkunft für B6.

### A8 – Discovery und JSON

Discovery-Nachrichten entstehen teils per `snprintf` in feste Puffer (B3), teils als `std::string` mit
Textsuche im eigenen Ergebnis (`find("\"p\":\"binary_sensor\"")`). Ein kleiner Schreiber, der Felder
anhängt, die Länge kennt und Überlauf meldet, für alle Entitäten; die Art eines Sensors als Struktur
(Plattform, Geräteklasse, Einheit, Zustandsklasse) statt als Textfragment. `STATS_LABELS` und die Werte in
`statsAppendJson()` laufen heute parallel mit magischen Indizes (`i >= 11 && i <= 13`); eine Tabelle
{Name, Typ, Nachkommastellen, Wert} ersetzt beides.

### A9 – Weboberfläche

Eine HTML-Datei mit rund 1000 Zeilen dichtem JavaScript, einzelne Zeilen über 400 Zeichen, keine Tests.
Der Versionsvergleich existiert zweimal (`newerBuild()` in der Seite, `updateNewer()` in der Firmware).
Vorschlag: Module unter `web/src/`, die `build_web.py` zusammenführt (das Skript bettet ohnehin schon die
Übersetzungen ein); reine Funktionen (`reading()`, `versionRank()`, `newerBuild()`) mit Node-Tests.

### A10 – Nebenläufigkeit festhalten

Drei Tasks (Main-Loop, Wärmepumpe, Web) plus Netz-Ereignisse und Watchdog-Timer teilen sich `config`, die
Werte, die Regelungsobjekte, `unitMeter`, `stats`. Geschützt sind die Werte und die serielle Leitung; der
Rest ist Konvention. Ein kurzes Dokument „wem gehört was, wer darf was aufrufen“ und Zusicherungen an den
Einstiegspunkten (Task-Handle prüfen) verhindern, dass B2, B4 und B12 l wiederkommen.

## 8. Tests, Build, CI

- **Neue native Tests:** Topics bei längster Base (B3); Discovery-JSON gültig und innerhalb der Grenzen;
  Konfiguration hin und zurück inklusive Grenzwerten und Größe (R2); Profil-Invarianten (A6);
  `LimitControl` bei Antwort vor geladenem RAM und mit Backoff (B5); Zähler-Plausibilität (R3);
  `queryRegistry`-Rahmen mit Echo, Teilantwort und Frist (B12 j); `LogRing`.
- **Generator:** „ein Name, ein Platz“ (B11); später Rollen und Einheiten (A3).
- **Weboberfläche:** Node-Tests der reinen Funktionen; ein Rauchtest gegen `scripts/webui_mock.py`.
- **Build:** Bibliotheken ohne feste Version (`PubSubClient`, `M5Unified`; `ArduinoJson@^7.2.0`) auf exakte
  Versionen festlegen; Warnungen einschalten und Größe der Images je Board im CI ausgeben.
- **Statische Prüfung:** `cppcheck` oder `clang-tidy` über die Arduino-freien Header (hätte B12 a und b
  gefunden).

## 9. Vorgeschlagene Reihenfolge

Der Maintainer hält den Kern bewusst klein; deshalb kleine, einzeln mergebare PRs, Fehler zuerst.

| Paket | Inhalt | Aufwand |
|---|---|---|
| 1 | B6 (Rückfrage, nur `true`, Herkunft im Ereignis), B12 a–e, g–k, n | klein |
| 2 | B3 (Topic- und Discovery-Puffer, Längentest) | klein |
| 3 | B1 (gehaltenes Ziel eigener Schlüssel und verzögert, Ereignisse zusammenfassen) | mittel |
| 4 | B2 (Schnappschuss statt Senden unter der Sperre, Serial-Spur weg) | mittel |
| 5 | B5 und R5 dritter Punkt (Plausibilität, Backoff, Entprellen) mit Tests | mittel |
| 6 | S1 (Origin/Header-Prüfung), B7 (Zeitzone), B8, B9 | klein bis mittel |
| 7 | B4 (Laufzeitzustand trennen, Restore mit Rückfrage und Version) – baut auf Paket 3 auf | mittel |
| 8 | R1 (Pin-Tabelle, Validierung, abgesicherter Start) mit G4 | groß |
| 9 | R4, R5 (Neustart-Politik, `expire_after`, Entität statt Löschen), R3 | mittel |
| 10 | GUI: G2, G3, G5–G9, G11–G14 in einzelnen PRs | je klein |
| 11 | Architektur A3 (Rollen), A5 (Feldtabelle), A6 (Identität), dann A1/A2/A4/A8/A9 schrittweise | groß |

Offene Fragen an den Maintainer vor Paket 8 bis 11: ob eine Pin-Tabelle je Board und ein abgesicherter
Start in den Kern sollen, und ob die Rollen im Generator gewünscht sind.
