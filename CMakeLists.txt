cmake_minimum_required(VERSION 3.20)
project(pokedex_host CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Pure, portable core logic (no hardware / SDL dependencies) ---
add_library(pokedex_core
  core/src/png_info.cpp
  core/src/layout.cpp
  core/src/render.cpp
  core/src/home_screen.cpp
  core/src/hit_test.cpp
  core/src/gesture.cpp
  core/src/card_browser.cpp
  core/src/battery.cpp
  core/src/deck.cpp
  core/src/deck_screen.cpp
  core/src/tcg.cpp
  core/src/wav.cpp
  core/src/keyboard.cpp
  core/src/base64.cpp
)
target_include_directories(pokedex_core PUBLIC core/include)

# --- Host unit tests (GoogleTest) ---
enable_testing()
find_package(GTest REQUIRED)

add_executable(pokedex_tests
  test/png_info_test.cpp
  test/layout_test.cpp
  test/render_test.cpp
  test/home_screen_test.cpp
  test/hit_test_test.cpp
  test/gesture_test.cpp
  test/card_browser_test.cpp
  test/battery_test.cpp
  test/deck_test.cpp
  test/deck_screen_test.cpp
  test/tcg_test.cpp
  test/wav_test.cpp
  test/keyboard_test.cpp
  test/base64_test.cpp
)
target_include_directories(pokedex_tests PRIVATE test)
target_link_libraries(pokedex_tests PRIVATE pokedex_core GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(pokedex_tests)

# --- SDL host app (visual backend; thin I/O glue over the tested core) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_image)

add_executable(pokedex_sdl
  host/main_sdl.cpp
  host/sdl_display.cpp
)
target_include_directories(pokedex_sdl PRIVATE host)
target_link_libraries(pokedex_sdl PRIVATE pokedex_core PkgConfig::SDL2)
