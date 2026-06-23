# Pokédex on M5Stack Tab5 — Project Plan

A voice-driven Pokémon card finder running on an **M5Stack Tab5** (ESP32-P4, 5″
1280×720 MIPI-DSI touchscreen, mic + speaker). Hold a mic button, speak, and the
device finds and shows matching Pokémon cards.

---

## Architecture

The guiding principle is a **portable, host-tested core** with thin,
swappable hardware/host backends behind a small `Display` interface. The same
rendering/logic code runs in an SDL window on the dev machine **and** on the
Tab5 — only the backend differs.

```
core/         Pure C++17, no hardware deps. Unit-tested with GoogleTest.
  Display          abstract drawing surface (the seam)
  geometry.h       Point / ImageSize / Rgb
  png_info         pngSize() — parse W/H from a PNG header
  layout           centerOrigin()
  render           renderCentered()  (clear → center → draw → present)
  home_screen      homeLayout() + renderHome()  (title / hero / mic)
  hit_test         contains()  (point-in-rect, for touch)
  gesture          detectSwipe()  (start/end -> Left/Right/Up/Down)
  card_browser     CardBrowser  (swipe -> next/prev card, up -> home)
  tcg              tcgQueryUrl()  (CardQuery -> pokemontcg.io URL, encoded)
  base64           base64Encode() / basicAuthHeader()  (STT HTTPS basic auth)
tools/voice_to_cards.py   host pipeline: phrase -> Claude -> TCG -> images
host/         SDL2 backend (SdlDisplay): a window for dev + offscreen→PNG.
  main_sdl.cpp     host preview app
firmware/     ESP-IDF app for the Tab5.
  main/main.cpp        app_main: bring-up, render, touch→record→playback loop
  main/m5gfx_display   M5GfxDisplay backend (M5Unified's M5.Display)
  main/assets/         PNGs embedded into flash
tools/gen_assets.py    generates title / mic / scaled-hero PNGs
test/         GoogleTest unit tests (currently 11, all green)
```

Build/test (host): `cmake -S . -B build -G Ninja && cmake --build build && ./build/pokedex_tests`
Preview (host):    `./build/pokedex_sdl --window`   (run from project root)

---

## What works today

- **Host TDD core** — 11 passing tests (png parse, centering, home layout,
  render-via-FakeDisplay, hit-test).
- **SDL preview** — portrait home screen renders in a window / to PNG.
- **Tab5 firmware** — flashes and runs:
  - Display reliably initialises in **portrait** (720×1280) via M5Unified.
  - Home screen shows **POKEDEX** title, **Jolteon**, and a red **mic button**.
  - **Press-and-hold the mic button → records; release → plays it back.**
    Recording is chunked (100 ms) so release stops within ~100 ms; speaker
    volume maxed; mic gain modest.

### Hardware / toolchain notes (hard-won)
- ESP-IDF v5.4 at `~/esp/esp-idf`; target `esp32p4`.
- Build: `source ~/esp/esp-idf/export.sh && cd firmware && idf.py build`.
- Flash: device is `/dev/ttyACM0` (USB-JTAG). User is in `dialout` but the
  shell predates that, so flash via `sg dialout -c "... idf.py -p /dev/ttyACM0 flash"`.
- **Use M5Unified, not m5gfx alone** — m5gfx alone autodetects the Tab5 but the
  panel init is flaky (`panel was not detected`) because it skips the board
  power-up sequence. `M5.begin()` powers the rails first → reliable.
- Touch: use `M5.Touch.getCount() > 0` for live press state (the controller's
  release *edge* flags are unreliable).
- Audio gotcha: a single big `M5.Mic.record()` makes `M5.Mic.end()` block until
  the whole DMA finishes → record in small chunks instead.
- Mic & speaker share the bus: `M5.Speaker.end()` before `M5.Mic.begin()` and
  vice versa.

---

## The plan (revised)

**Drop the on-device rules/STT approach. Use voice → (transcription) → Claude
API → card search → display.**

Pipeline:
1. **Capture** spoken query on the Tab5 (done).
2. **Transcribe** audio to text. NOTE: the Claude API is text+image, not audio,
   so a speech-to-text step is still required (e.g. a cloud STT service). The
   device POSTs the recorded PCM/WAV over WiFi and gets back text.
3. **Interpret + search via Claude API** — send the transcript to Claude, which
   maps it to a pokemontcg.io query (or directly selects matching cards). Claude
   handles the "broad matching" ("fairy", "healing", "trainer", …) far better
   than hand-built rules.
4. **Fetch** matching card images from pokemontcg.io.
5. **Display** the card(s) on the Tab5 (portrait — cards are portrait).

Card data source: **pokemontcg.io** REST API (rich query + hi-res images).

### Roadmap
- [x] Toolchain, display bring-up, portrait, home screen, mic record/playback.
- [x] After playback, show a SINGLE hard-coded card (Morelull). Card-display path proven.
- [x] Query mapping (phrase → TCG query) — host pipeline `tools/voice_to_cards.py`
      using Claude `claude-opus-4-8` with structured output. Ready to run once
      `ANTHROPIC_API_KEY` is set (`pip install anthropic requests`).
- [x] Swipe + multi-card browsing logic — `gesture` + `card_browser`, TDD'd
      (swipe L/R between cards, swipe up → home).
- [x] TCG query URL building — `tcg::tcgQueryUrl`, TDD'd.
- [x] **STT decided + working (host):** self-hosted **whisper.cpp** server
      (`~/whisper.cpp`, model `ggml-base.en.bin`). Shared contract: POST WAV to
      `/inference` → text. URL via `POKEDEX_STT_URL` (default localhost).
      `tools/stt_client.py` + `--wav` in `voice_to_cards.py`.
- [x] **Full host chain proven live:** WAV → whisper.cpp → Claude
      (`claude-opus-4-8`) → pokemontcg.io → real cards + images. Verified with
      "fairy", "trainer", "healing".
- [x] **Build-time secrets:** `cmake/gen_secrets.cmake` generates a git-ignored
      `firmware/main/secrets.h` (WIFI_SSID/WIFI_PASSWORD/ANTHROPIC_API_KEY/STT_URL)
      from the secret files on every configure.
- [x] **WiFi working on the Tab5.** ESP32-P4 ↔ ESP32-C6 via **esp_hosted (SDIO)**
      — `esp_wifi_remote` 1.6.1 + `esp_hosted` 2.12.9. Connects to the FritzBox
      and gets an IP (e.g. 192.168.178.84); IP shown on screen. See the hosted
      SDIO config in `firmware/sdkconfig.defaults`.
- [x] On-device HTTP pipeline implemented (`firmware/main/net.cpp`): record →
      WAV (`wavHeader`, TDD'd) → multipart POST to `STT_URL` → Claude HTTPS
      (`claude-opus-4-8`, structured output, "<type> energy" fix) → pokemontcg.io
      → card image URLs. Uses `esp_http_client` + cJSON + TLS cert bundle.
- [x] Device results screen: downloads each card (scaled-to-fit) and lets the
      user swipe via the tested `gesture` + `card_browser` (left/right = cards,
      up = home). Boots clean with WiFi; **pending a live voice end-to-end test.**

- [x] **Animated busy indicator** — `busyTask` (FreeRTOS) cycles "Transcribing." →
      ".." → "..." behind blocking STT/Claude/TCG calls, guarded by a display
      mutex (`s_disp_mtx`); `startBusy`/`stopBusy` (stopBusy fences the animator).
- [x] **Card counter shown before the image** — `showCardImage` draws "i / N"
      immediately, then downloads, so the user sees which card is loading.
- [x] **On-screen keyboard input** — keyboard icon below the mic on the home
      screen opens a QWERTY screen (letters only, Backspace, full-width GO).
      Core logic TDD'd in `core/keyboard` (`keyboardLayout`/`keyAt`/`applyKey`,
      11 tests); firmware `runKeyboardScreen` draws it and, on GO, plumbs the
      typed phrase through the same `runSearchForPhrase` path the mic uses.
      Empty GO cancels back to home. Host test count now 42.

### Next / polish
- Live-verify voice→cards AND keyboard→cards on device; tune mic gain / swipe
  threshold; the keyboard screen's text-box-to-keys gap could tighten.
- Error retries on the device; cache downloaded cards.
- Refine Claude prompt edge cases; consider showing card name/text overlay.

### Known refinements
- **"<type> energy" → 0 cards.** Claude maps it to `types:Fairy supertype:Energy`,
  but TCG energy cards don't carry a `types` field. Refine the system prompt so
  "<type> energy" → `name:"<type> energy"` (e.g. `name:"fairy energy"`).

### Hardware notes — Tab5 WiFi (hard-won)
- WiFi is the C6 over **esp_hosted SDIO**, not eppp. Select it with
  `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y` + `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`.
- The SDIO **pins are NOT directly settable** — `ESP_HOSTED_SDIO_PIN_*` are derived
  from a board preset. Select `CONFIG_ESP_HOSTED_P4_DEV_BOARD_NONE=y` (opens the
  GPIO ranges) and set the editable PRIV symbols:
  `ESP_HOSTED_PRIV_SDIO_PIN_{CLK=12,CMD=13,D0=11}_SLOT_1`,
  `ESP_HOSTED_PRIV_SDIO_PIN_{D1=10,D2=9,D3=8}_4BIT_BUS_SLOT_1`,
  `ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=15`, `SLOT_1`, `RESET_ACTIVE_HIGH`, `4_BIT_BUS`.
- Benign boot warning: `Version mismatch: Host [2.12.0] > Co-proc [0.0.0]` — the
  C6 slave fw doesn't report a version; it connects fine. Reflash the C6 slave
  only if RPC timeouts ever appear.

### Hardware notes — Tab5 display (panel detection race)
- M5GFX 0.2.23 selects the Tab5 panel from a **flaky I2C touch-FW read** and only
  *logs* the reliable DSI panel ID, so the panel intermittently "wasn't detected"
  at boot (esp. warm/RTS resets). Fixed in a vendored local fork
  `firmware/components/m5gfx` (overrides the registry dep): on DSI ID 0x71/0x23,
  set `hit_st7123` and use the ST7123 panel. Now 3/3 reliable on the warm-reset
  path. Fork is trimmed to ~6 MB (CJK fonts + SDL-only splash images removed);
  see `firmware/components/m5gfx/PATCH_NOTES.md`. Not upstreamed (by request).

### Pending decisions / blockers
- **STT is hosted on the internet** at `https://dev.uoawen.com/stt/inference`:
  whisper.cpp bound to localhost, fronted by nginx (HTTPS + basic auth, real
  Let's Encrypt cert so the device's CA-bundle verification passes). Clients
  authenticate with `stt-auth.txt` / `POKEDEX_STT_AUTH` (`user:password`). The
  firmware sends `Authorization: Basic …` only when `STT_AUTH` is non-empty, so
  a no-auth LAN box still works unchanged. See README "Hosting the STT server".
- All previously-blocking items resolved: API key present, STT live on the
  internet, WiFi connects on-device, pokemontcg.io reachable.

---

## Immediate next step (this iteration)

Ignore the speech entirely. After the record→playback completes, display the
**Morelull** card (`sm3-97`, a Basic **Fairy** Pokémon from Burning Shadows),
scaled to fit the portrait screen. Card image is bundled as an embedded asset
(`assets/card_morelull.png`, 700×977) and drawn with the already-tested
`renderCentered()`.
