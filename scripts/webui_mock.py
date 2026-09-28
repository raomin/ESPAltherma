"""Serves web/index.html with a fake device API, to work on the web interface without an ESP.

    python scripts/webui_mock.py [--port 8080] [--fresh]

--fresh starts like a device right after flashing (no WiFi, no MQTT, no model).
The responses mirror include/webserver.h; values change every few seconds.
"""

import argparse
import json
import os
import random
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import webpage  # noqa: E402  (the page with its translations)

MODELS = [
    ("Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW)", "G", "I"),
    ("Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O)", "G", "I"),
    ("Altherma(EBLA-EDLA D series 4-8kW Monobloc)", "G", "I"),
    ("Altherma(LT_CA_CB_04-08kW)", "L", "I"),
    ("Altherma(Hybrid)", "L", "I"),
    ("PROTOCOL_S", "S", "S"),
    ("PROTOCOL_S_ROTEX", "R", "S"),
]
CATALOG = [
    (0x10, 0, 217, "Operation Mode", True),
    (0x10, 1, 304, "Defrost Operation", True),
    (0x20, 0, 105, "R1T-Outdoor air temp.", True),
    (0x21, 0, 105, "INV primary current (A)", True),
    (0x30, 0, 152, "INV frequency (rps)", True),
    (0x61, 2, 105, "Leaving water temp. before BUH (R1T)", True),
    (0x61, 4, 105, "Leaving water temp. after BUH (R2T)", True),
    (0x61, 8, 105, "Inlet water temp.(R4T)", True),
    (0x61, 10, 105, "DHW tank temp. (R5T)", True),
    (0x62, 12, 105, "Flow sensor (l/min)", True),
    (0x62, 14, 105, "Water pressure", True),
    (0x20, 2, 105, "Discharge pipe temp.", False),
    (0x20, 4, 105, "Heat exchanger mid-temp.", False),
    (0x62, 0, 307, "Not in use", False),
    (0x63, 2, 215, "I/U EEPROM (3rd digit)", False),
]


def key(reg, off, conv, size=2):
    return reg << 24 | off << 16 | (conv & 0x3FF) << 4 | size


class Device:
    def __init__(self, fresh, ethernet=False):
        self.start = time.time()
        self.ethernet = ethernet
        self.lock = threading.Lock()
        self.config = {
            "wifi": {"ssid": "" if fresh else "HomeWiFi", "has_pwd": not fresh, "static": False, "ip": "", "gateway": "", "subnet": "", "dns1": "", "dns2": ""},
            "hostname": "ESPAltherma",
            "mqtt": {"server": "" if fresh else "192.168.1.10", "port": 1883, "user": "", "tls": False, "client_id": "ESPAltherma-dev", "has_pwd": False},
            "hp": {"protocol": "I", "frequency": 30000, "rx": 16, "tx": 17, "model": "" if fresh else MODELS[0][0], "confirmed": False, "detected_key": "", "labels": []},
            "relays": {"therm_pin": -1, "therm_high": True, "sg1_pin": -1, "sg2_pin": -1, "sg_high": True, "safety_pin": -1, "safety_high": True},
            "output": {"one_topic": False, "one_topic_prefix": "espaltherma/OneATTR/", "json_table": False, "no_log": False, "debug_serial": False},
            "admin": {"has_pwd": False},
            "telemetry": {"enabled": False, "install_id": "3f2a9c1be07d4411"},
        }
        self.survey_until = time.time() + 6
        self.discover_until = 0
        self.mqtt_test_until = 0
        self.log = "ESPAltherma mock started\n"

    def status(self):
        c = self.config
        wifi_ok = bool(c["wifi"]["ssid"]) and not self.ethernet  # Ethernet boards keep the WiFi as a standby
        link = "ethernet" if self.ethernet else "wifi" if wifi_ok else "none"
        st = {
            "fw": "2.0.0-mock", "board": "wt32-eth01" if self.ethernet else "esp32", "uptime": int(time.time() - self.start),
            "heap": 182000, "heap_min": 151000, "hostname": c["hostname"],
            "net": {"link": link, "ip": "192.168.1.43" if self.ethernet else "192.168.1.42" if wifi_ok else "0.0.0.0"},
            "wifi": {"ssid": c["wifi"]["ssid"], "connected": wifi_ok, "ip": "192.168.1.42" if wifi_ok else "0.0.0.0", "rssi": -58,
                     "bssid": "80:3F:5D:67:A1:91" if wifi_ok else "", "channel": 12 if wifi_ok else 0, "last_disconnect": "",
                     "ap": link == "none", "ap_ssid": "ESPAltherma-1A2B", "ap_ip": "192.168.4.1"},
            "mqtt": {"configured": bool(c["mqtt"]["server"]), "connected": bool(c["mqtt"]["server"]) and link != "none", "server": c["mqtt"]["server"], "state": 0},
            "hp": {"generic": True, "protocol": c["hp"]["protocol"], "model": c["hp"]["model"], "confirmed": c["hp"]["confirmed"],
                   "values": len(self.values()), "last_poll": 12 if c["hp"]["model"] else -1,
                   "survey_running": time.time() < self.survey_until, "survey_done": time.time() >= self.survey_until},
            "detect": {"model": MODELS[0][0], "family": "G", "confidence": "low"},
            "telemetry_available": True,
            "reset_reason": "restart by the firmware", "restart_cause": "Settings changed from the web interface",
        }
        if self.ethernet:
            st["eth"] = {"up": True, "ip": "192.168.1.43", "mac": "A8:03:2A:11:22:33", "speed": 100, "full_duplex": True}
        return st

    def events(self):
        now = int(time.time())
        up = int(time.time() - self.start)
        items = [
            (0, 3, 57, "Boot #58: restart by the firmware, firmware 2.0.0-mock"),
            (now - up - 5, 312, 57, "Restart: No network for 5 minutes (WiFi 20:23:51:97:82:12 ch 6 -84 dBm, last disconnect NO_AP_FOUND)"),
            (now - up - 20, 297, 57, "WiFi: disconnected, NO_AP_FOUND (201), 18 more attempts failed"),
            (now - up - 300, 12, 57, "WiFi: disconnected, BEACON_TIMEOUT (200)"),
            (now - up - 312, 0, 57, "Boot #57: power on, firmware 2.0.0-mock"),
        ]
        events = [{"t": now - up + 3, "up": 3, "boot": 58, "text": "Boot #58: restart by the firmware, firmware 2.0.0-mock"},
                  {"t": now - up + 9, "up": 9, "boot": 58, "text": "WiFi: joined access point 6A:48:B8:EA:44:08, channel 9"},
                  {"t": now - up + 10, "up": 10, "boot": 58, "text": "Online over WiFi: 192.168.1.42, -52 dBm"}]
        events += [{"t": t, "up": u, "boot": b, "text": x} for t, u, b, x in items[1:]]
        events.sort(key=lambda e: -e["t"])
        return {"boot": 58, "uptime": up, "clock": True, "events": events}

    def survey(self):
        if time.time() < self.survey_until:
            return {}
        return {"v": 1, "fw": "2.0.0-mock", "board": "esp32", "protocol": "I",
                "key": "I|63:017081710306|60:9482|CAP:5A|11:017053480100|00:1234",
                "id": {"iu_eeprom": "017081710306", "iu_as": "AS1708171-3", "iu_sw": "9482", "iu_cap": 90, "iu_opt": 2, "iu_eeprom_ver": 0,
                       "iu_code": "ABCD", "ou_eeprom": "017053480100", "ou_pn": "1P705348-1", "ou_mpu": "1234", "ou_cap": 160},
                "regs": {"60": "0000000000ABCD5A00000000000291820700", "00": "050100010101000001011234", "A0": "00000000000000000000000000", "65": None},
                "detect": {"model": MODELS[0][0], "family": "G", "confidence": "low", "method": "rules", "margin": 0,
                           "candidates": [{"model": MODELS[0][0], "score": 88}, {"model": MODELS[1][0], "score": 88}, {"model": MODELS[2][0], "score": 88}]}}

    def selected_keys(self, model):
        labels = self.config["hp"]["labels"]
        if model == self.config["hp"]["model"] and labels:
            return set(labels)
        return {key(r, o, c) for r, o, c, _, rec in CATALOG if rec}

    def values(self):
        model = self.config["hp"]["model"]
        if not model:
            return []
        sel = self.selected_keys(model)
        out = []
        for r, o, c, l, _ in CATALOG:
            if key(r, o, c) not in sel:
                continue
            v = {217: "Heating", 304: "OFF"}.get(c, "%.1f" % (20 + random.random() * 30))
            out.append({"l": l, "v": v, "r": r})
        return out


class Handler(BaseHTTPRequestHandler):
    device = None

    def log_message(self, *args):
        pass

    def send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else (body if isinstance(body, str) else json.dumps(body)).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return json.loads(self.rfile.read(n) or b"{}")

    def do_GET(self):
        d = self.device
        url = urlparse(self.path)
        p = url.path
        if p == "/":
            return self.send(200, webpage.render(ROOT).encode("utf-8"), "text/html")
        if p == "/api/status":
            return self.send(200, d.status())
        if p == "/api/config":
            return self.send(200, d.config)
        if p == "/api/detect":
            return self.send(200, d.survey())
        if p == "/api/events":
            return self.send(200, d.events())
        if p == "/api/models":
            return self.send(200, [{"name": n, "family": f, "protocol": pr} for n, f, pr in MODELS])
        if p == "/api/catalog":
            model = parse_qs(url.query).get("model", [d.config["hp"]["model"]])[0]
            sel = d.selected_keys(model)
            return self.send(200, [{"k": key(r, o, c), "r": r, "o": o, "c": c, "l": l, "rec": rec, "sel": key(r, o, c) in sel} for r, o, c, l, rec in CATALOG])
        if p == "/api/values":
            return self.send(200, d.values())
        if p == "/api/wifi/scan":
            return self.send(200, {"running": False, "networks": [{"ssid": "HomeWiFi", "rssi": -52, "secure": True}, {"ssid": "Neighbour", "rssi": -80, "secure": True}]})
        if p == "/api/discover":
            running = time.time() < d.discover_until
            return self.send(200, {"running": running, "done": not running, "hosts": [] if running else [
                {"name": "homeassistant", "ip": "192.168.1.10", "port": 1883, "ha": True, "mqtt": True}]})
        if p == "/api/mqtt/test":
            if time.time() < d.mqtt_test_until:
                return self.send(200, {"result": "running"})
            if not d.config["mqtt"]["server"].startswith("192.168."):
                return self.send(200, {"result": "error", "reason": "broker unreachable", "state": -2})
            return self.send(200, {"result": "ok"})
        if p == "/api/log":
            return self.send(200, d.log, "text/plain")
        if p == "/api/telemetry":
            return self.send(200, {"install_id": d.config["telemetry"]["install_id"], "model": d.config["hp"]["model"],
                                   "confirmed": d.config["hp"]["confirmed"], "report": d.survey()})
        if p == "/events":
            return self.events()
        self.send(404, {"ok": False, "error": "not found"})

    def events(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        try:
            while True:
                for event, data in (("values", self.device.values()), ("status", self.device.status())):
                    self.wfile.write(("event: %s\ndata: %s\n\n" % (event, json.dumps(data))).encode())
                self.wfile.write(("event: log\ndata: Querying register 0x61... CRC OK!\n\n").encode())
                self.wfile.flush()
                time.sleep(5)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_POST(self):
        d = self.device
        p = urlparse(self.path).path
        if p == "/api/config":
            data = self.body()
            with d.lock:
                for section, values in data.items():
                    if isinstance(values, dict):
                        target = d.config.setdefault(section, {})
                        for k, v in values.items():
                            if k == "pwd":
                                target["has_pwd"] = bool(v)
                            else:
                                target[k] = v
                    else:
                        d.config[section] = values
            return self.send(200, {"ok": True, "reboot": "relays" in data})
        if p == "/api/detect":
            d.survey_until = time.time() + 5
            return self.send(200, {"ok": True})
        if p == "/api/discover":
            d.discover_until = time.time() + 3
            return self.send(200, {"ok": True})
        if p == "/api/mqtt/test":
            d.mqtt_test_until = time.time() + 2
            return self.send(200, {"ok": True})
        if p in ("/api/reboot", "/api/factory-reset", "/update"):
            return self.send(200, {"ok": True})
        self.send(404, {"ok": False, "error": "not found"})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--fresh", action="store_true", help="start like a freshly flashed device")
    ap.add_argument("--ethernet", action="store_true", help="a board connected by Ethernet")
    args = ap.parse_args()
    Handler.device = Device(args.fresh, args.ethernet)
    print("Mock ESPAltherma on http://localhost:%d" % args.port)
    ThreadingHTTPServer(("", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
