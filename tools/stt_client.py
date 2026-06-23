#!/usr/bin/env python3
"""Speech-to-text client for the self-hosted whisper.cpp server.

One endpoint, same contract for the host preview and the Tab5: POST a WAV to
`/inference`, get the transcript back. The URL is configurable so the service
can move (e.g. to the hosted internet endpoint) without code changes.

  export POKEDEX_STT_URL="https://dev.uoawen.com/stt/inference"   # default
  export POKEDEX_STT_AUTH="user:password"     # basic auth, if the endpoint needs it

Usage:
    python tools/stt_client.py recording.wav
"""
import argparse
import os
import sys

import requests

# Re-point these (env vars) when the STT service moves. The hosted endpoint is
# fronted by nginx basic auth; a no-auth LAN box just leaves POKEDEX_STT_AUTH unset.
DEFAULT_STT_URL = os.environ.get(
    "POKEDEX_STT_URL", "https://dev.uoawen.com/stt/inference"
)
DEFAULT_STT_AUTH = os.environ.get("POKEDEX_STT_AUTH", "")


def _auth_tuple(auth: str | None):
    """"user:password" -> (user, password) for requests; "" / None -> None."""
    auth = DEFAULT_STT_AUTH if auth is None else auth
    if not auth:
        return None
    user, _, password = auth.partition(":")
    return (user, password)


def transcribe(
    wav_path: str,
    url: str | None = None,
    timeout: int = 60,
    auth: str | None = None,
) -> str:
    """POST a WAV file to the whisper.cpp server and return the transcript."""
    url = url or DEFAULT_STT_URL
    with open(wav_path, "rb") as f:
        resp = requests.post(
            url,
            files={"file": (os.path.basename(wav_path), f, "audio/wav")},
            data={"response_format": "json", "temperature": "0"},
            auth=_auth_tuple(auth),
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
    ap.add_argument("--stt-auth", default=DEFAULT_STT_AUTH,
                    help='basic auth as "user:password" (default: $POKEDEX_STT_AUTH)')
    args = ap.parse_args()
    print(transcribe(args.wav, args.stt_url, auth=args.stt_auth))
    return 0


if __name__ == "__main__":
    sys.exit(main())
