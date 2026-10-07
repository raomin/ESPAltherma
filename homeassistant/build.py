"""Builds dist/espaltherma-card.js from src/espaltherma-card.js, with the card's texts translated by the ESPAltherma
web page translations (web/i18n/<lang>.json) so the drawing reads the same in Home Assistant.

    python homeassistant/build.py
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
LANGS = ["fr", "de", "it", "es"]

src = open(os.path.join(HERE, "src", "espaltherma-card.js"), encoding="utf-8").read()
# Texts of the card: data-t attributes, tag labels and tr(hass, "...") calls
texts = set(re.findall(r'data-t="([^"]+)"', src)) | set(re.findall(r'tr\(hass, "([^"]+)"', src))
texts |= {t for t in re.findall(r'\["([^"]+)", "\w+", \d+, \d+, \d+\]', src)}
i18n, missing = {}, {}
for lang in LANGS:
    d = json.load(open(os.path.join(ROOT, "web", "i18n", lang + ".json"), encoding="utf-8"))
    i18n[lang] = {t: d[t] for t in sorted(texts) if d.get(t)}
    missing[lang] = sorted(t for t in texts if not d.get(t))
out = src.replace("/*I18N*/{}", json.dumps(i18n, ensure_ascii=False, separators=(",", ":")))
os.makedirs(os.path.join(HERE, "dist"), exist_ok=True)
open(os.path.join(HERE, "dist", "espaltherma-card.js"), "w", encoding="utf-8", newline="\n").write(out)
print("%d texts; missing translations: %s" % (len(texts), {k: v for k, v in missing.items() if v}))
