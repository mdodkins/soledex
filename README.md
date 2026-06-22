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
`/inference`, get the transcript back. The URL is configurable via
`POKEDEX_STT_URL` (default `http://127.0.0.1:8080/inference`) so the service can
move to a dedicated server later without code changes.

The engine itself is **not** vendored here (it's a large external dependency).
Set it up once — clone, build the server, and fetch the base.en model:

```bash
git clone https://github.com/ggml-org/whisper.cpp ~/whisper.cpp
cd ~/whisper.cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target whisper-server
./models/download-ggml-model.sh base.en      # -> models/ggml-base.en.bin
```

Then start the server (leave it running while you use the device):

```bash
~/whisper.cpp/build/bin/whisper-server \
  -m ~/whisper.cpp/models/ggml-base.en.bin -l en --host 0.0.0.0 --port 8080
# --host 0.0.0.0 so the Tab5 can reach it over the LAN
# (use 127.0.0.1 for host-only testing)
```

### Running it on a dedicated server (systemd)

To move STT off this dev machine, build whisper.cpp on the server as above
(clone to `/opt/whisper.cpp`, fetch `base.en`), then install the unit shipped at
[`tools/whisper-stt.service`](tools/whisper-stt.service). Adjust its `User=` and
the `/opt/whisper.cpp` paths to match the box, then:

```bash
# On the server (paths/user as in the unit file):
sudo useradd --system --home /opt/whisper.cpp --shell /usr/sbin/nologin whisper
sudo chown -R whisper: /opt/whisper.cpp
sudo cp tools/whisper-stt.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now whisper-stt      # start now + on every boot

systemctl status whisper-stt                 # check it's up
journalctl -u whisper-stt -f                  # follow logs
curl -F file=@recording.wav http://<server-ip>:8080/inference   # smoke test
```

Then re-point the two clients at the server's address (nothing else changes —
the `/inference` contract is identical):

- **Host tools:** `export POKEDEX_STT_URL="http://<server-ip>:8080/inference"`
- **Tab5 firmware:** put `http://<server-ip>:8080/inference` in `stt-url.txt`
  (git-ignored, project root). It's baked into `secrets.h` at CMake-configure
  time — rebuild and flash the firmware to apply.

Open port 8080 to the LAN only (e.g. `sudo ufw allow from 192.168.0.0/16 to any
port 8080`); whisper-server has no auth, so don't expose it to the internet.

Transcribe, or run the full chain from a recording:

```bash
python tools/stt_client.py recording.wav                 # just STT
export ANTHROPIC_API_KEY="$(cat claude-api-key.txt)"
python tools/voice_to_cards.py --wav recording.wav --limit 5   # STT → Claude → cards
```
