# Pokédex (M5Stack Tab5)

A voice-driven Pokémon card finder for the **M5Stack Tab5** (ESP32-P4, 5″
1280×720 touchscreen, mic + speaker). Hold the mic button and say what you're
after ("fairy energy", "trainer", "healing"…) — or tap the keyboard button and
type it on the on-screen QWERTY — and matching cards appear to swipe through
(left/right between cards, up to go home).

Under the hood: the query (spoken → transcribed by a self-hosted whisper.cpp, or
typed) goes to the Claude API, which maps it to a [pokemontcg.io](https://pokemontcg.io)
search; the device fetches the matching card images over WiFi and displays them.

See **[plan.md](plan.md)** for architecture, status, and the roadmap.

## Layout

```
core/      Portable C++17 logic (no hardware deps), unit-tested with GoogleTest
host/      SDL2 backend — preview the UI in a window on your dev machine
firmware/  ESP-IDF app for the Tab5 (M5Unified backend)
tools/     Asset generation + the voice→cards pipeline
test/      GoogleTest unit tests
```

## Build & test (host)

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/pokedex_tests          # unit tests
./build/pokedex_sdl --window   # preview the UI (run from project root)
```

## Firmware (Tab5)

ESP-IDF v5.4, target `esp32p4`. The secret files below are baked into a
git-ignored `firmware/main/secrets.h` at CMake-configure time, so set those up
first. WiFi runs over the on-board ESP32-C6 via esp_hosted (SDIO).

```bash
source ~/esp/esp-idf/export.sh
cd firmware
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Notes (hard-won — see plan.md for the full list):

- If your shell predates being added to the `dialout` group, prefix the flash
  with `sg dialout -c "idf.py -p /dev/ttyACM0 flash"`.
- The board may enumerate as `/dev/ttyACM0` **or** `/dev/ttyACM1` after a reset —
  check `ls /dev/ttyACM*` if a flash can't find the port.
- `firmware/components/m5gfx` is a **vendored, trimmed M5GFX fork** (reliable
  Tab5 panel detection) — it's committed on purpose; don't delete it.

## Secrets (git-ignored — never commit)

Credentials live in plain-text files at the project root, listed in
`.gitignore`. Keep adding entries as new networks/keys are needed.

| File | Format | Holds |
|------|--------|-------|
| `claude-api-key.txt` | the API key on one line | Anthropic API key for the Claude calls |
| `wifi-passwords.txt` | `SSID,password` — one network per line | WiFi credentials (incl. the FritzBox) |
| `stt-url.txt` *(optional)* | the STT URL on one line | overrides the default `https://dev.uoawen.com/stt/inference` |
| `stt-auth.txt` *(optional)* | `user:password` on one line | basic-auth creds for the hosted STT endpoint |

`wifi-passwords.txt` example:

```
Zen Internet - FRITZ!Box IA,<password>
Some Other Network,<password>
```

Both files are in `.gitignore` and must never be committed. To run the host
pipeline, the key is read from `claude-api-key.txt` (or `ANTHROPIC_API_KEY`):

```bash
export ANTHROPIC_API_KEY="$(cat claude-api-key.txt)"
pip install anthropic requests
python tools/voice_to_cards.py "fairy energy"
```

## Speech-to-text (self-hosted whisper.cpp)

The device can't run STT locally, so we host one **whisper.cpp** endpoint and
both the host preview and the Tab5 use the same contract: POST a WAV to
`/inference`, get the transcript back. It is served on the internet at
**`https://dev.uoawen.com/stt/inference`** behind nginx basic auth (see
[Hosting the STT server](#hosting-the-stt-server)). The URL and credentials are
configurable, so the service can move without code changes:

- **Host tools:** `POKEDEX_STT_URL` (default `https://dev.uoawen.com/stt/inference`)
  and `POKEDEX_STT_AUTH` (`user:password`).
- **Tab5 firmware:** `stt-url.txt` and `stt-auth.txt` at the project root
  (git-ignored), baked into `secrets.h` at CMake-configure time.

The engine itself is **not** vendored here (it's a large external dependency).
Set it up once — clone, build the server, and fetch the base.en model:

```bash
git clone https://github.com/ggml-org/whisper.cpp ~/whisper.cpp
cd ~/whisper.cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target whisper-server
./models/download-ggml-model.sh base.en      # -> models/ggml-base.en.bin
```

Start it for a quick local test (for the always-on deployment, see
[Hosting the STT server](#hosting-the-stt-server)):

```bash
~/whisper.cpp/build/bin/whisper-server \
  -m ~/whisper.cpp/models/ggml-base.en.bin -l en --host 127.0.0.1 --port 8080
```

### Hosting the STT server

whisper-server has **no auth of its own**, so to reach it over the internet we
bind it to localhost and front it with nginx (HTTPS + basic auth). The live
deployment serves `https://dev.uoawen.com/stt/inference`.

Build whisper.cpp on the server (clone to `/opt/whisper.cpp`, fetch `base.en`),
then run it as a localhost-bound service via the unit shipped at
[`tools/whisper-stt.service`](tools/whisper-stt.service) (adjust `User=`/paths to
match the box):

```bash
sudo useradd --system --home /opt/whisper.cpp --shell /usr/sbin/nologin whisper
sudo chown -R whisper: /opt/whisper.cpp
sudo cp tools/whisper-stt.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now whisper-stt      # start now + on every boot
systemctl status whisper-stt                 # check it's up
curl -F file=@recording.wav http://127.0.0.1:8080/inference   # local smoke test
```

Front it with nginx using the snippet at
[`tools/nginx-soledex-stt.conf`](tools/nginx-soledex-stt.conf) (add the
`location /stt/` block to your domain's HTTPS server, create the
`.htpasswd-soledex` credential file as documented in the snippet, reload nginx):

```bash
curl -u user:password https://your-domain/stt/inference -F file=@recording.wav
```

Use a **real (Let's Encrypt) cert** for the domain — the Tab5 verifies the
endpoint against the Mozilla CA bundle, so a self-signed cert is rejected
on-device (`certbot certonly --webroot -w /var/www/html -d your-domain`).

Then point the clients at the endpoint (the `/inference` contract is identical):

- **Host tools:** `export POKEDEX_STT_URL="https://dev.uoawen.com/stt/inference"`
  and `export POKEDEX_STT_AUTH="user:password"`.
- **Tab5 firmware:** put the URL in `stt-url.txt` and `user:password` in
  `stt-auth.txt` (both git-ignored, project root). They are baked into
  `secrets.h` at CMake-configure time — rebuild and flash to apply.

> For a LAN-only box instead, bind whisper-server to `0.0.0.0`, firewall port
> 8080 to the local subnet (`sudo ufw allow from 192.168.0.0/16 to any port
> 8080`), skip nginx, and leave `stt-auth.txt`/`POKEDEX_STT_AUTH` unset.

Transcribe, or run the full chain from a recording:

```bash
python tools/stt_client.py recording.wav                 # just STT
export ANTHROPIC_API_KEY="$(cat claude-api-key.txt)"
python tools/voice_to_cards.py --wav recording.wav --limit 5   # STT → Claude → cards
```
