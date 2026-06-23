#!/usr/bin/env python3
"""Transcript -> Claude -> pokemontcg.io search -> card images.

Proves the host side of the voice->cards pipeline. On the Tab5 the same three
steps run as: record audio -> STT -> this Claude call -> TCG fetch -> display.
Speech-to-text is NOT done here (Claude is text+image, not audio) — pass the
already-transcribed phrase as the argument.

Setup:
    pip install anthropic requests
    export ANTHROPIC_API_KEY=sk-ant-...

Usage:
    python tools/voice_to_cards.py "fairy energy"
    python tools/voice_to_cards.py "cards that heal" --download out/ --limit 5
"""
import argparse
import json
import os
import sys
import urllib.parse

import requests

import stt_client

MODEL = "claude-opus-4-8"
TCG_URL = "https://api.pokemontcg.io/v2/cards"

# Teaches Claude to turn a spoken phrase into a pokemontcg.io Lucene query.
SYSTEM = """You convert a spoken Pokemon card search into a query for the \
pokemontcg.io v2 API `q` parameter (Lucene syntax).

Rules and examples:
- A bare type -> types:  e.g. "fairy" -> types:Fairy ; "water" -> types:Water
- A supertype word -> supertype:  "trainer" -> supertype:Trainer ; \
"energy" -> supertype:Energy
- Combine type + supertype: "fairy energy" -> supertype:Energy types:Fairy
- An effect/keyword (not a type) -> search card text across attacks, abilities \
and rules. e.g. "healing"/"heal" -> \
(attacks.text:heal* OR abilities.text:heal* OR rules:heal*)
- A Pokemon name -> name:  "pikachu" -> name:Pikachu
Prefer the most specific interpretation. Output the query only via the tool."""

# Guarantees a parseable JSON object back (structured outputs).
SCHEMA = {
    "type": "object",
    "properties": {
        "q": {"type": "string", "description": "pokemontcg.io q= query"},
        "explanation": {"type": "string", "description": "one-line rationale"},
    },
    "required": ["q", "explanation"],
    "additionalProperties": False,
}


def phrase_to_query(phrase: str) -> dict:
    """Ask Claude to map the spoken phrase to a TCG query (structured output)."""
    import anthropic  # imported here so the TCG-only path works without the SDK

    client = anthropic.Anthropic()
    resp = client.messages.create(
        model=MODEL,
        max_tokens=1024,
        system=SYSTEM,
        output_config={"format": {"type": "json_schema", "schema": SCHEMA}},
        messages=[{"role": "user", "content": phrase}],
    )
    text = next(b.text for b in resp.content if b.type == "text")
    return json.loads(text)


def search_cards(query: str, limit: int) -> list[dict]:
    """Run the query against pokemontcg.io and return card dicts."""
    url = f"{TCG_URL}?q={urllib.parse.quote(query)}&pageSize={limit}"
    r = requests.get(url, timeout=20)
    r.raise_for_status()
    return r.json().get("data", [])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("phrase", nargs="?", help="the spoken phrase (or use --wav)")
    ap.add_argument("--wav", help="WAV file to transcribe via the STT server first")
    ap.add_argument("--stt-url", default=stt_client.DEFAULT_STT_URL)
    ap.add_argument("--stt-auth", default=stt_client.DEFAULT_STT_AUTH,
                    help='STT basic auth as "user:password" (default: $POKEDEX_STT_AUTH)')
    ap.add_argument("--limit", type=int, default=10)
    ap.add_argument("--download", metavar="DIR", help="download card images here")
    ap.add_argument("--query", help="skip Claude; use this q= directly")
    args = ap.parse_args()

    # Full chain entry point: a recording becomes the phrase via STT.
    if args.wav and not args.phrase:
        args.phrase = stt_client.transcribe(args.wav, args.stt_url, auth=args.stt_auth)
        print(f'transcribed: "{args.phrase}"')
    if not args.phrase:
        ap.error("provide a phrase or --wav")

    if args.query:
        mapped = {"q": args.query, "explanation": "(provided directly)"}
    else:
        mapped = phrase_to_query(args.phrase)
    print(f'phrase: "{args.phrase}"')
    print(f'query : {mapped["q"]}   ({mapped["explanation"]})')

    cards = search_cards(mapped["q"], args.limit)
    print(f"found : {len(cards)} cards")
    for i, c in enumerate(cards):
        img = c.get("images", {}).get("large", "")
        print(f"  [{i}] {c['name']:<24} {c.get('supertype','')}/"
              f"{','.join(c.get('types', []) or [])}  {img}")

    if args.download and cards:
        os.makedirs(args.download, exist_ok=True)
        for i, c in enumerate(cards):
            img = c.get("images", {}).get("large")
            if not img:
                continue
            dest = os.path.join(args.download, f"card_{i:02d}_{c['id']}.png")
            with open(dest, "wb") as f:
                f.write(requests.get(img, timeout=20).content)
            print(f"  saved {dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
