"""Explicitly enable the expert API and exchange complete hexadecimal X10A frames."""
import argparse
import base64
import json
import time
import urllib.request
from pathlib import Path


class ExpertClient:
    def __init__(self, url, password_file):
        self.url = url.rstrip("/")
        password = Path(password_file).read_text(encoding="utf-8").strip()
        self.headers = {
            "Authorization": "Basic " + base64.b64encode(("admin:" + password).encode()).decode(),
            "Content-Type": "application/json",
            "X-ESPAltherma-Expert": "1",
        }

    def request(self, path, body=None):
        req = urllib.request.Request(
            self.url + path,
            data=None if body is None else json.dumps(body).encode(),
            headers=self.headers,
        )
        with urllib.request.urlopen(req, timeout=10) as response:
            return json.load(response)

    def enable(self, enabled):
        return self.request("/api/expert", {"enabled": enabled})

    def exchange(self, frame, timeout_ms=500, idle_ms=100):
        submitted = self.request("/api/expert/x10a", {
            "frame": frame.hex(), "timeout_ms": timeout_ms, "idle_ms": idle_ms,
        })
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            result = self.request("/api/expert/x10a")
            if result["id"] != submitted["id"]:
                raise RuntimeError("Result replaced by another client; do not repeat a write automatically")
            if result["done"]:
                return result
            time.sleep(0.025)
        raise TimeoutError("Transaction not completed; inspect status before issuing any further write")


def frame(command, payload=b""):
    """Protocol I command frame; use exchange directly for other/raw formats."""
    if len(payload) > 253:
        raise ValueError("payload too long")
    data = bytes([len(payload) + 2, command]) + payload
    return data + bytes([(~sum(data)) & 255])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--password-file", required=True)
    parser.add_argument("--timeout-ms", type=int, default=500)
    parser.add_argument("--idle-ms", type=int, default=100)
    parser.add_argument("action", choices=["enable", "disable", "status", "frame"])
    parser.add_argument("hex", nargs="?")
    args = parser.parse_args()
    client = ExpertClient(args.url, args.password_file)
    if args.action in ("enable", "disable"):
        result = client.enable(args.action == "enable")
    elif args.action == "status":
        result = client.request("/api/expert")
    else:
        if not args.hex:
            parser.error("frame requires a complete hexadecimal frame, including checksum")
        result = client.exchange(bytes.fromhex(args.hex), args.timeout_ms, args.idle_ms)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
