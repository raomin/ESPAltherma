"""Serves the web flasher (flasher/) on http://localhost:8000 to test it before publishing.

    python scripts/build_flasher.py --build   # firmwares + manifests
    python scripts/serve_flasher.py [--port 8000]

Open the page in Chrome or Edge: Web Serial works on localhost without https. Responses are not cached,
so a rebuilt firmware is picked up right away. Only reachable from this computer.
"""

import argparse
import functools
import os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class NoCacheHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8000)
    args = ap.parse_args()
    directory = os.path.join(ROOT, "flasher")
    if not os.path.isdir(os.path.join(directory, "firmware")):
        print("No firmware yet: run python scripts/build_flasher.py --build first.")
    handler = functools.partial(NoCacheHandler, directory=directory)
    print("ESPAltherma installer on http://localhost:%d (Chrome or Edge)" % args.port)
    ThreadingHTTPServer(("127.0.0.1", args.port), handler).serve_forever()


if __name__ == "__main__":
    main()
