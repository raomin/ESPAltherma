"""Checks the order of the web server's routes in include/webserver.h.

The server hands a request to the first route whose path the request equals or continues: "/api/memory" also takes
"/api/memory/dump". A route registered after one that covers it, for the same method, is never reached: it builds,
passes every test and answers with the other route's reply on the board.

    python scripts/check_routes.py --check
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "include" / "webserver.h"
ROUTE = re.compile(r'webServer\.on\(\s*"([^"]+)"\s*(?:,\s*(HTTP_[A-Z]+))?')


def routes(text):
    """(line, path, method) of every route, in the order the server tries them."""
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        for match in ROUTE.finditer(line):
            found.append((number, match.group(1), match.group(2) or "HTTP_ANY"))
    return found


def covers(first, later):
    """True when a request for the later route is taken by the first one."""
    (_, path, method), (_, later_path, later_method) = first, later
    same_method = method == later_method or "HTTP_ANY" in (method, later_method)
    return same_method and (later_path == path or later_path.startswith(path.rstrip("/") + "/")) and path != "/"


def shadowed(found):
    return [(first, later) for i, first in enumerate(found) for later in found[i + 1:] if covers(first, later)]


def main():
    found = routes(SOURCE.read_text(encoding="utf-8"))
    if len(found) < 10:
        print("check_routes: only %d routes found in %s, the pattern no longer matches" % (len(found), SOURCE.name))
        return 1
    bad = shadowed(found)
    for first, later in bad:
        print("%s:%d: %s %s is never reached: %s at line %d comes first and takes it"
              % (SOURCE.name, later[0], later[2], later[1], first[1], first[0]))
    if bad:
        print("Register the longer path first.")
        return 1
    print("check_routes: %d routes, none hidden by an earlier one" % len(found))
    return 0


if __name__ == "__main__":
    sys.exit(main())
