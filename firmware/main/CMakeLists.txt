# Generate secrets.h from the git-ignored secret files (WiFi, API key, STT URL).
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/gen_secrets.cmake")

# Reuse the portable, host-tested core sources directly (no duplication).
set(CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../core")

idf_component_register(
  SRCS
    "main.cpp"
    "m5gfx_display.cpp"
    "wifi.cpp"
    "net.cpp"
    "${CORE_DIR}/src/png_info.cpp"
    "${CORE_DIR}/src/layout.cpp"
    "${CORE_DIR}/src/render.cpp"
    "${CORE_DIR}/src/home_screen.cpp"
    "${CORE_DIR}/src/hit_test.cpp"
    "${CORE_DIR}/src/gesture.cpp"
    "${CORE_DIR}/src/card_browser.cpp"
    "${CORE_DIR}/src/tcg.cpp"
    "${CORE_DIR}/src/wav.cpp"
    "${CORE_DIR}/src/keyboard.cpp"
  INCLUDE_DIRS
    "."
    "${CORE_DIR}/include"
  EMBED_FILES
    "assets/title_soledex.png"
    "assets/jolteon_mid.png"
    "assets/mic_button.png"
    "assets/keyboard_button.png"
    "assets/card_morelull.png"
)
