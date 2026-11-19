"""Builds the demo of the web interface: the page the firmware carries, with a board that is not there.

    python scripts/build_demo.py [--version X.Y.Z] [--scenes demo/scenes] [--out flasher/demo]

The page is web/index.html with its translations, as build_web.py embeds it in the firmware. One script is added
at the top of it: demo/board.js, which answers the page in place of a board, from recordings of a real one: one
per scene (SCENES below), <scenes>/<id>.json, each made and cleaned with scripts/record_demo.py. The scenes that
have no file are left out. The result is one file, <out>/index.html, published with the installer site.

Local test: python scripts/build_demo.py, then python scripts/serve_flasher.py and open http://localhost:8000/demo/
"""

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import webpage  # noqa: E402


def firmware_version():
    with open(os.path.join(ROOT, "include", "version.h"), encoding="utf-8") as f:
        m = re.search(r'#define ESPALTHERMA_VERSION "([^"]+)"', f.read())
    return m.group(1) if m else "dev"


def page_paths(page):
    """The board's paths the page asks for."""
    return set(re.findall(r"'(/api/[a-zA-Z0-9_/-]+)", page)) | {"/events", "/update"}


def board_paths(board):
    """The paths demo/board.js answers."""
    return set(re.findall(r"'(/api/[a-zA-Z0-9_/-]+)':", board)) | {"/events", "/update"}


EVENTS_KEPT = 24  # what a board keeps of its history

# The scenes a visitor chooses from, in the order of the banner. The demo opens on the first of OPENS_ON that has
# a file: that one also gives the answers that do not move (settings, values of the model, survey...).
SCENES = [("standby", "Standby"), ("dhw", "Hot water"), ("heating", "Room heating")]
OPENS_ON = ["heating", "dhw", "standby"]


def changes(before, after):
    """What changed from one status to the next, to write a status of the stream in a tenth of its size:
    {key: new value}, an object changed inside as its own changes. None when a key is gone (written whole then)."""
    out = {}
    if set(before) - set(after):
        return None
    for k, v in after.items():
        if k in before and before[k] == v:
            continue
        if isinstance(v, dict) and isinstance(before.get(k), dict):
            inner = changes(before[k], v)
            if inner is None:
                return None
            out[k] = inner
        else:
            out[k] = v
    return out


def compact(stream):
    """The stream with each status as its changes since the status before (the first one, and any that lost a
    field, whole: "full"), and each list of values as {place in the list: new text} once the list was given whole."""
    out, last, listed = [], None, None
    for e in stream:
        if e["event"] == "values":
            same = listed is not None and [(x["l"], x["r"]) for x in listed] == [(x["l"], x["r"]) for x in e["data"]]
            out.append({"at": e["at"], "event": "values", "data": {str(i): x["v"] for i, x in enumerate(e["data"]) if x["v"] != listed[i]["v"]}}
                       if same else e)
            listed = e["data"]
            continue
        if e["event"] != "status":
            out.append(e)
            continue
        diff = changes(last, e["data"]) if last is not None else None
        out.append({"at": e["at"], "event": "status", "data": e["data"], "full": 1} if diff is None
                   else {"at": e["at"], "event": "status", "data": diff})
        last = e["data"]
    return out


def load_scenes(folder):
    scenes, found = [], {}
    for name, title in SCENES:
        path = os.path.join(folder, name + ".json")
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8") as f:
            found[name] = json.load(f)
        scenes.append({"id": name, "title": title, "stream": compact(found[name]["stream"])})
    if not scenes:
        raise SystemExit("build_demo: no scene in %s (expected %s)" % (folder, ", ".join(n + ".json" for n, _ in SCENES)))
    opens = [name for name in OPENS_ON if name in found][0]
    base = found[opens]
    return {"recorded": base["recorded"], "answers": base["answers"], "names": base.get("names", {}), "opens": opens,
            "scenes": scenes}


def version_before(version):
    """2.0.0-beta.32 -> 2.0.0-beta.31: the version the demo's board was updated from. The same when it cannot tell."""
    m = re.match(r"^(.*-(?:alpha|beta|rc)\.)(\d+)$", version)
    return m.group(1) + str(int(m.group(2)) - 1) if m and int(m.group(2)) > 1 else version


def history(recording, version):
    """The event history of demo/history.json, as the board answers /api/events: the newest first."""
    with open(os.path.join(ROOT, "demo", "history.json"), encoding="utf-8") as f:
        story = json.load(f)
    status = recording["answers"]["/api/status"]
    words = {"fw": version, "old": version_before(version), "board": status["board"], "ip": status["net"]["ip"],
             "bssid": status["wifi"]["bssid"], "channel": status["wifi"]["channel"], "broker": status["mqtt"]["server"]}
    boots, now = story["boots"], recording["recorded"]
    starts = [now - (b["started"] if "started" in b else status["uptime"]) for b in boots]
    events = []
    for i, b in enumerate(boots):
        number = story["first_boot"] + i
        lines = [[0, "Boot #%d: %s, firmware %s" % (number, b["reason"], b["firmware"])]] + b["events"]
        for up, text in lines:
            if up < 0:  # counted back from the next boot
                up += starts[i + 1] - starts[i]
            events.append({"t": starts[i] + up, "up": up, "boot": number, "text": text.format_map(words)})
    if events != sorted(events, key=lambda e: e["t"]) or events[-1]["t"] > now:
        raise SystemExit("build_demo: demo/history.json is not in the order of time, or goes beyond now")
    return {"boot": story["first_boot"] + len(boots) - 1, "uptime": status["uptime"], "clock": True,
            "events": events[::-1][:EVENTS_KEPT]}


def build(version, scenes_folder):
    page = webpage.render(ROOT)
    with open(os.path.join(ROOT, "demo", "board.js"), encoding="utf-8") as f:
        board = f.read()
    missing = sorted(page_paths(page) - board_paths(board))
    if missing:
        raise SystemExit("build_demo: the page asks for %s, which demo/board.js does not answer" % ", ".join(missing))
    recording = load_scenes(scenes_folder)
    # The history is written for the demo (demo/history.json): the recorded one is whatever happened to that board
    recording["answers"]["/api/events"] = history(recording, version)
    print("build_demo: scenes %s" % ", ".join("%s (%d events, %.1f min)" % (s["id"], len(s["stream"]), s["stream"][-1]["at"] / 60)
                                              for s in recording["scenes"]))
    recording = json.dumps(recording, ensure_ascii=False, separators=(",", ":"))
    for mark, value in (("/*RECORDING*/null", recording), ("/*VERSION*/''", json.dumps(version))):
        if mark not in board:
            raise SystemExit("build_demo: %s not found in demo/board.js" % mark)
        board = board.replace(mark, value.replace("</", "<\\/"))
    if "<head>" not in page:
        raise SystemExit("build_demo: <head> not found in web/index.html")
    # First in the page: the board has to be there before the page's own script asks for it
    return page.replace("<head>", "<head>\n<script>\n" + board + "</script>", 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default=firmware_version())
    ap.add_argument("--scenes", default=os.path.join(ROOT, "demo", "scenes"))
    ap.add_argument("--out", default=os.path.join(ROOT, "flasher", "demo"))
    args = ap.parse_args()
    page = build(args.version, args.scenes)
    os.makedirs(args.out, exist_ok=True)
    path = os.path.join(args.out, "index.html")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(page)
    print("build_demo: %s, %d KB, version %s" % (os.path.relpath(path, ROOT), len(page.encode("utf-8")) // 1024, args.version))


if __name__ == "__main__":
    main()
