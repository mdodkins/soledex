# Local M5GFX fork (vendored)

Overrides the registry `m5stack/m5gfx` (0.2.23). Two changes:

1. **Fix (src/M5GFX.cpp):** Tab5 panel selection trusts the reliable DSI panel
   ID (0x71/0x23) instead of a race-prone I2C touch-FW read, so the display is
   detected even when that read times out at boot.
2. **Trim (size only):** removed CJK font data (efont tw/ja/kr/cn, IPA Japanese)
   and SDL-only boot-splash images — neither is used by this device build.
