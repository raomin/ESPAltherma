"""Records what a board answers to its web page, for the demo (scripts/build_demo.py): every answer the page reads,
then the event stream for some minutes. Only reads: nothing is changed on the board, and no WiFi scan is asked.

    python scripts/record_demo.py <board address> <raw.json> [--minutes 10]
    python scripts/record_demo.py --timeline <raw.json>
    python scripts/record_demo.py --cut <raw.json> <part.json> --from 95 --to 110
    python scripts/record_demo.py --clean <part.json> demo/scenes/heating.json

A scene of the demo is some minutes of one thing the heat pump does. The simple way to get them all is one long
recording (a night), --timeline to see when the compressor ran and where the three-way valve was, then --cut for
each scene (minutes from the start of the recording): a part where the unit is fully at it from its first second
to its last, since the demo plays it in a loop.

The raw file holds what the board said, with the addresses and names of the network it is on: keep it out of the
repository. --clean writes the copy the demo uses, where those are replaced (see clean()).
"""

import argparse
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request

# The answers the page reads with a GET that changes nothing. Not /api/wifi/scan: asking it starts a scan.
PATHS = ["/api/status", "/api/config", "/api/values", "/api/models", "/api/catalog", "/api/events", "/api/detect",
         "/api/log", "/api/heartbeat", "/api/telemetry", "/api/memory", "/api/memory/dump/status", "/api/discover",
         "/api/mqtt/test"]


def get(host, path):
    with urllib.request.urlopen("http://%s%s" % (host, path), timeout=15) as r:
        body = r.read().decode("utf-8", "replace")
        return json.loads(body) if "json" in (r.headers.get("Content-Type") or "") else body


def write(result, path):
    # beside the file, then put in its place: a recording stopped while it is saved keeps its last save
    with open(path + ".part", "w", encoding="utf-8", newline="\n") as f:
        json.dump(result, f, ensure_ascii=False, separators=(",", ":"))
    os.replace(path + ".part", path)


def record(host, minutes, output=None):
    """The answers, then the stream for the minutes asked. A long recording survives what a night brings: it is
    written to output every five minutes, and a stream the board closed (or a network that dropped) is opened again."""
    answers = {}
    for path in PATHS:
        try:
            answers[path] = get(host, path)
            print("%-28s %d bytes" % (path, len(json.dumps(answers[path]))))
        except Exception as e:  # an older firmware without this route: the demo answers without it
            print("%-28s not recorded: %s" % (path, e))
    names = catalog_names(host, answers)
    stream, unreadable, start = [], [], time.time()
    result = {"recorded": int(start), "answers": answers, "names": names, "stream": stream, "unreadable": unreadable}
    end, saved = start + minutes * 60, start
    # the board only answers the stream to a client that asks for one
    stream_request = urllib.request.Request("http://%s/events" % host, headers={"Accept": "text/event-stream"})
    while time.time() < end:
        event, data = None, []
        try:
            with urllib.request.urlopen(stream_request, timeout=120) as r:
                while time.time() < end:
                    raw = r.readline()
                    if not raw:
                        print("the board closed the stream after %.0f s" % (time.time() - start))
                        break
                    line = raw.decode("utf-8", "replace").rstrip("\r\n")
                    if line.startswith("event:"):
                        event = line[6:].strip()
                    elif line.startswith("data:"):
                        data.append(line[5:].lstrip(" "))
                    elif not line and event:
                        text = "\n".join(data)
                        try:
                            stream.append({"at": round(time.time() - start, 1), "event": event,
                                           "data": text if event == "log" else json.loads(text)})
                        except ValueError as e:  # left out, and said: the page could not read it either
                            unreadable.append({"at": round(time.time() - start, 1), "event": event, "why": str(e), "text": text})
                            print("unreadable %s event at %.0f s: %s" % (event, time.time() - start, e))
                        event, data = None, []
                    if output and time.time() - saved > 300:
                        write(result, output)
                        saved = time.time()
        except OSError as e:  # the network, or a board that restarted
            print("stream lost after %.0f s: %s" % (time.time() - start, e))
        if time.time() < end:
            time.sleep(5)
    print("%d events in %d minutes" % (len(stream), minutes))
    return result


def catalog_names(host, answers):
    """{language: {label: name}}: the names of the model's values in the languages of the page."""
    model = (answers.get("/api/status") or {}).get("hp", {}).get("model", "")
    names = {}
    for lang in ("fr", "de", "it", "es", "nl"):
        try:
            rows = get(host, "/api/catalog?model=%s&lang=%s" % (urllib.parse.quote(model), lang))
            names[lang] = {row["l"]: row["n"] for row in rows if row.get("n")}
        except Exception as e:
            print("names in %s not recorded: %s" % (lang, e))
    return names


# What tells whose network this is, by the name of its field: replaced by a plain value of the same kind
FIELDS = {"ssid": "HomeWiFi", "ap_ssid": "ESPAltherma-1A2B", "server": "192.168.1.10", "user": "", "client_id": "ESPAltherma-demo",
          "install_id": "0000000000000000", "gateway": "", "subnet": "", "dns1": "", "dns2": "", "ntp": ""}
IP = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")
MAC = re.compile(r"\b(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}\b")


def clean(raw):
    """The recording without the network's own names and addresses. Addresses and MACs are replaced wherever they
    are written, in the event history and the log too, each by the same stand-in every time."""
    ips, macs = {}, {}

    def text(s):
        s = IP.sub(lambda m: m.group(0) if m.group(0) in ("0.0.0.0", "192.168.4.1") else
                   ips.setdefault(m.group(0), "192.168.1.%d" % (40 + len(ips))), s)
        return MAC.sub(lambda m: macs.setdefault(m.group(0).upper(), "A8:03:2A:00:00:%02X" % (16 + len(macs))), s)

    def walk(value, name=None):
        if isinstance(value, dict):
            return {k: walk(v, k) for k, v in value.items()}
        if isinstance(value, list):
            return [walk(v, name) for v in value]
        if isinstance(value, str):
            if name in FIELDS and value:
                return FIELDS[name]
            return text(value)
        return value

    out = walk(raw)
    print("%d addresses and %d MACs replaced" % (len(ips), len(macs)))
    return out


def moments(raw):
    """(seconds, what changed) each time the compressor or the pump starts or stops, or the valve, the backup
    heater or the defrost changes, read in the values of the stream."""
    watched = (("INV frequency", "compressor"), ("Water pump", "pump"), ("3way valve", "valve to the hot water"),
               ("BUH Step1", "backup heater"), ("Defrost", "defrost"))
    state, out = {}, []
    for e in raw["stream"]:
        if e["event"] != "values":
            continue
        for item in e["data"]:
            for word, name in watched:
                if word in item["l"]:
                    try:
                        on = float(item["v"]) > 0
                    except ValueError:
                        on = item["v"] in ("ON", "1")
                    if state.get(name) is not None and state[name] != on:
                        out.append((e["at"], "%s %s" % (name, "on" if on else "off")))
                    elif name not in state:
                        out.append((e["at"], "%s is %s" % (name, "on" if on else "off")))
                    state[name] = on
    return out


def timeline(raw):
    start = raw["recorded"]
    print("recorded from %s, %d minutes" % (time.strftime("%F %T", time.localtime(start)), raw["stream"][-1]["at"] / 60))
    for at, what in moments(raw):
        print("%6.1f min  %s  %s" % (at / 60, time.strftime("%T", time.localtime(start + at)), what))


def cut(raw, first, last):
    """The minutes first..last of a recording, as a recording of its own."""
    a, b = first * 60, last * 60
    stream = [dict(e, at=round(e["at"] - a, 1)) for e in raw["stream"] if a <= e["at"] < b]
    out = dict(raw, recorded=raw["recorded"] + int(a), stream=stream, unreadable=[])
    answers = dict(raw["answers"])
    for kind in ("status", "values"):  # what the board answered at the start of the part
        found = [e["data"] for e in stream if e["event"] == kind]
        if found:
            answers["/api/" + kind] = found[0]
    out["answers"] = answers
    print("%d events, %.0f minutes" % (len(stream), (stream[-1]["at"] if stream else 0) / 60))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", help="the board's address, or with --clean the raw recording")
    ap.add_argument("output", nargs="?")
    ap.add_argument("--minutes", type=float, default=10)
    ap.add_argument("--clean", action="store_true")
    ap.add_argument("--timeline", action="store_true")
    ap.add_argument("--cut", action="store_true")
    ap.add_argument("--from", dest="first", type=float, default=0, help="--cut: minutes from the start")
    ap.add_argument("--to", dest="last", type=float, default=1e9)
    args = ap.parse_args()
    if args.timeline or args.clean or args.cut:
        with open(args.source, encoding="utf-8") as f:
            raw = json.load(f)
    if args.timeline:
        return timeline(raw)
    if not args.output:
        ap.error("the file to write is missing")
    if args.clean:
        result = clean(raw)
    elif args.cut:
        result = cut(raw, args.first, args.last)
    else:
        result = record(args.source, args.minutes, args.output)
    write(result, args.output)
    print("%s written" % args.output)


if __name__ == "__main__":
    sys.exit(main())
