"""Checks the translations of the device web page (web/i18n/) and of the installer page (flasher/i18n/).

    python scripts/check_i18n.py            # report missing, empty and unused strings
    python scripts/check_i18n.py --check    # the same, exit 1 if a language is incomplete (CI)
    python scripts/check_i18n.py --update   # add the new strings (empty) and drop the unused ones in every file

English is the source text: the keys are the English strings, as the pages find them:
  - text of the page (outside <script>, <style>, <pre> and elements marked translate="no"), whitespace collapsed,
  - placeholder attributes,
  - the whole inner HTML of elements marked data-th (text with links or bold inside a sentence),
  - literal strings in t('...') and N_('...') calls of the page scripts,
  - for the installer, the board notes of flasher/boards.json.
A translation keeps the {name} placeholders of its English string.
"""

import json
import os
import re
import sys
from html.parser import HTMLParser

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
from webpage import LANGUAGES  # noqa: E402

SKIP = {"script", "style", "pre"}
VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"}


def norm(s):
    return re.sub(r"\s+", " ", s).strip()


class Strings(HTMLParser):
    """Text nodes, placeholders and data-th blocks of a page, in order."""

    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.source = source
        self.keys = []
        self.stack = []  # (tag, skipped)
        self.in_body = False
        self.th_depth = None
        self.lines = [0]
        for line in source.splitlines(keepends=True):
            self.lines.append(self.lines[-1] + len(line))

    def source_offset(self):
        line, col = self.getpos()
        return self.lines[line - 1] + col

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "body":
            self.in_body = True
        skipped = bool(self.stack and self.stack[-1][1]) or tag in SKIP or a.get("translate") == "no" or self.th_depth is not None
        if self.in_body and not skipped and a.get("placeholder"):
            self.keys.append(norm(a["placeholder"]))
        if self.in_body and not skipped and "data-th" in a:
            # inner HTML: from the end of this start tag to its matching end tag
            start = self.source_offset() + len(self.get_starttag_text())
            depth, pos = 1, start
            pattern = re.compile(r"<(/?)%s\b[^>]*>" % tag, re.I)
            while depth:
                m = pattern.search(self.source, pos)
                depth += -1 if m.group(1) else 1
                pos = m.end()
            self.keys.append(norm(self.source[start:m.start()]))
            self.th_depth = len(self.stack)
            skipped = True
        if tag not in VOID:
            self.stack.append((tag, skipped))

    def handle_endtag(self, tag):
        while self.stack:
            t, _ = self.stack.pop()
            if self.th_depth is not None and len(self.stack) == self.th_depth:
                self.th_depth = None
            if t == tag:
                break

    def handle_data(self, data):
        if self.in_body and norm(data) and not (self.stack and self.stack[-1][1]):
            self.keys.append(norm(data))


def script_strings(source):
    out = []
    for script in re.findall(r"<script>(.*?)</script>", source, re.S):
        for m in re.finditer(r"\b(?:t|N_)\('((?:[^'\\]|\\.)*)'", script):
            out.append(norm(m.group(1).replace("\\'", "'").replace("\\\\", "\\")))
    return out


def page_keys(folder):
    path = os.path.join(ROOT, folder, "index.html")
    with open(path, encoding="utf-8") as f:
        source = f.read()
    parser = Strings(source)
    parser.feed(source)
    keys = parser.keys + script_strings(source)
    if folder == "flasher":
        with open(os.path.join(ROOT, "flasher", "boards.json"), encoding="utf-8") as f:
            keys += [norm(b["note"]) for b in json.load(f)]
    seen, ordered = set(), []
    for k in keys:
        if k not in seen:
            seen.add(k)
            ordered.append(k)
    return ordered


def placeholders(s):
    return sorted(re.findall(r"\{(\w+)\}", s))


def main():
    update = "--update" in sys.argv
    check = "--check" in sys.argv
    problems = 0
    for folder in ["web", "flasher"]:
        keys = page_keys(folder)
        print("%s: %d strings" % (folder, len(keys)))
        for code in LANGUAGES:
            path = os.path.join(ROOT, folder, "i18n", code + ".json")
            current = {}
            if os.path.exists(path):
                with open(path, encoding="utf-8") as f:
                    current = json.load(f)
            missing = [k for k in keys if k not in current]
            empty = [k for k in keys if current.get(k) == ""]
            unused = [k for k in current if k not in keys]
            broken = [k for k in keys if current.get(k) and placeholders(current[k]) != placeholders(k)]
            print("  %s: %d missing, %d empty, %d unused, %d with other placeholders"
                  % (code, len(missing), len(empty), len(unused), len(broken)))
            for k in (missing + empty)[:5]:
                print("      to translate: %s" % k)
            for k in broken:
                print("      placeholders differ: %s -> %s" % (k, current[k]))
            problems += len(missing) + len(empty) + len(unused) + len(broken)
            if update:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8", newline="\n") as f:
                    json.dump({k: current.get(k, "") for k in keys}, f, ensure_ascii=False, indent=1)
                    f.write("\n")
    if check and problems:
        sys.exit(1)


if __name__ == "__main__":
    main()
