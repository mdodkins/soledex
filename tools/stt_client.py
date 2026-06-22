#!/usr/bin/env python3
"""Speech-to-text client for the self-hosted whisper.cpp server.

One endpoint, same contract for the host preview and the Tab5: POST a WAV to
`/inference`, get the transcript back. The URL is configurable so we can move
the service from this dev machine to a real server later without code changes.

  export POKEDEX_STT_URL="http://<host>:8080/inference"   # default: localhost

Usage:
    python tools/stt_client.py recording.wav
"""
import argparse
import os
import sys

import requests

# Re-point this (env var) when the STT service moves off this dev machine.
DEFAULT_STT_URL = os.environ.get(
    "POKEDEX_STT_URL", "http://127.0.0.1:8080/inference"
)


def transcribe(wav_path: str, url: str | None = None, timeout: int = 60) -> str:
    """POST a WAV file to the whisper.cpp server and return the transcript."""
    url = url or DEFAULT_STT_URL
    with open(wav_path, "rb") as f:
        resp = requests.post(
            url,
            files={"file": (os.path.basename(wav_path), f, "audio/wav")},
            data={"response_format": "json", "temperature": "0"},
            timeout=timeout,
        )
    resp.raise_for_status()
    try:
        return resp.json().get("text", "").strip()
    except ValueError:
        return resp.text.strip()  # server configured for plain-text responses


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("wav", help="path to a WAV file (16kHz mono works best)")
    ap.add_argument("--stt-url", default=DEFAULT_STT_URL)
    args = ap.parse_args()
    print(transcribe(args.wav, args.stt_url))
    return 0


if __name__ == "__main__":
    sys.exit(main())
