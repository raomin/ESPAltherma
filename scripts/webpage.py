"""The device web page with its translations: web/index.html, where the placeholder /*I18N*/{} becomes the content of
web/i18n/<lang>.json ({"English text": "translation"}). Used by build_web.py (firmware) and webui_mock.py (local test).
"""

import json
import os

LANGUAGES = ["fr", "de", "it", "es"]  # English is the source text
PLACEHOLDER = "/*I18N*/{}"


def translations(root, folder):
    """{lang: {English: translation}} of the <folder>/i18n/*.json files that exist."""
    out = {}
    for code in LANGUAGES:
        path = os.path.join(root, folder, "i18n", code + ".json")
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                # Untranslated entries (empty) fall back to English in the page
                out[code] = {k: v for k, v in json.load(f).items() if v}
    return out


def render(root):
    """The page, with the translations inlined."""
    with open(os.path.join(root, "web", "index.html"), encoding="utf-8") as f:
        page = f.read()
    if PLACEHOLDER not in page:
        raise SystemExit("web/index.html: translation placeholder %s not found" % PLACEHOLDER)
    data = json.dumps(translations(root, "web"), ensure_ascii=False, separators=(",", ":"), sort_keys=True)
    return page.replace(PLACEHOLDER, data.replace("</", "<\\/"))
