#!/usr/bin/env python3
"""
Translate lang/en.yml to target language using DeepL API.
Usage:
  python3 .agents/scripts/translate.py --lang de --api-key $DEEPL_KEY
  python3 .agents/scripts/translate.py --lang all --api-key $DEEPL_KEY
"""
import os
import sys
import yaml
import requests
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LANG_DIR = ROOT / "lang"
EN_FILE = LANG_DIR / "en.yml"

# All supported languages (ISO 639-1)
ALL_LANGS = [
    "de", "fr", "es", "pt", "it", "nl", "ja", "ko", "zh", "ru", "pl", "tr", "uk"
]

DEEPL_API = "https://api-free.deepl.com/v2/translate"

def load_en():
    with open(EN_FILE, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)

def translate_text(text, target_lang, api_key):
    """Translate a single string via DeepL."""
    if not text or not text.strip():
        return text

    # Preserve placeholders
    placeholders = {}
    def protect(m):
        key = f"__PH_{len(placeholders)}__"
        placeholders[key] = m.group(0)
        return key
    protected = re.sub(r"\{[^}]+\}", protect, text)

    payload = {
        "auth_key": api_key,
        "text": protected,
        "target_lang": target_lang.upper(),
        "tag_handling": "xml",  # helps preserve structure
    }

    resp = requests.post(DEEPL_API, data=payload, timeout=30)
    if resp.status_code != 200:
        raise RuntimeError(f"DeepL error {resp.status_code}: {resp.text}")

    result = resp.json()["translations"][0]["text"]

    # Restore placeholders
    for key, val in placeholders.items():
        result = result.replace(key, val)
    return result

def translate_file(target_lang, api_key, dry_run=False):
    en_data = load_en()
    out_file = LANG_DIR / f"{target_lang}.yml"

    # Load existing translations
    existing = {}
    if out_file.exists():
        with open(out_file, "r", encoding="utf-8") as f:
            existing = yaml.safe_load(f) or {}

    translated = {}
    for key, value in en_data.items():
        if not isinstance(value, str):
            translated[key] = value
            continue

        if key in existing and existing[key]:
            # Keep existing human translation
            translated[key] = existing[key]
        else:
            # Machine translate
            try:
                translated[key] = translate_text(value, target_lang, api_key)
            except Exception as e:
                print(f"Warning: failed to translate '{key}': {e}")
                translated[key] = value  # fallback to English

    if not dry_run:
        with open(out_file, "w", encoding="utf-8") as f:
            yaml.dump(translated, f, allow_unicode=True, sort_keys=False)
        print(f"Wrote {out_file} ({len(translated)} keys)")

def main():
    import argparse
    import re

    parser = argparse.ArgumentParser()
    parser.add_argument("--lang", required=True, choices=ALL_LANGS + ["all"])
    parser.add_argument("--api-key", required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    langs = ALL_LANGS if args.lang == "all" else [args.lang]

    for lang in langs:
        print(f"Translating to {lang}...")
        translate_file(lang, args.api_key, args.dry_run)

if __name__ == "__main__":
    main()