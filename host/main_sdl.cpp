#include <SDL.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "pokedex/home_screen.h"
#include "pokedex/render.h"
#include "sdl_display.h"

namespace {

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

}  // namespace

// Host preview of the Pokedex home screen (title + Jolteon + mic button) on a
// 1280x720 (Tab5-sized) surface, using the same pokedex::renderHome that runs
// on the device.
//
// Usage: pokedex_sdl [--assets DIR] [--window] [--save out.png]
int main(int argc, char** argv) {
  std::string assets = "firmware/main/assets";
  bool window = false;
  bool card = false;
  std::string save_path;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--window") {
      window = true;
    } else if (arg == "--card") {
      card = true;  // preview the card screen instead of the home screen
    } else if (arg == "--assets" && i + 1 < argc) {
      assets = argv[++i];
    } else if (arg == "--save" && i + 1 < argc) {
      save_path = argv[++i];
    }
  }

  std::vector<uint8_t> title = ReadFile(assets + "/title_soledex.png");
  std::vector<uint8_t> hero = ReadFile(assets + "/jolteon_mid.png");
  std::vector<uint8_t> mic = ReadFile(assets + "/mic_button.png");
  std::vector<uint8_t> kbd = ReadFile(assets + "/keyboard_button.png");
  std::vector<uint8_t> card_png = ReadFile(assets + "/card_morelull.png");
  if (title.empty() || hero.empty() || mic.empty() || kbd.empty() ||
      card_png.empty()) {
    std::fprintf(stderr, "could not read assets from %s\n", assets.c_str());
    return 1;
  }

  // Portrait, matching the Tab5 (Pokemon cards are portrait).
  pokedex::SdlDisplay display(720, 1280, window);
  if (!display.ok()) {
    return 1;
  }

  auto render = [&] {
    return card ? pokedex::renderCentered(display, card_png.data(),
                                          card_png.size())
                : pokedex::renderHome(display, title.data(), title.size(),
                                      hero.data(), hero.size(), mic.data(),
                                      mic.size(), kbd.data(), kbd.size());
  };

  if (!render()) {
    std::fprintf(stderr, "render failed (bad PNG?)\n");
    return 1;
  }

  if (!save_path.empty()) {
    std::printf(display.savePng(save_path) ? "saved %s\n" : "savePng failed\n",
                save_path.c_str());
  }

  if (window) {
    std::printf("showing window — close it or press Esc to exit\n");
    bool running = true;
    while (running) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
          running = false;
        } else if (e.type == SDL_KEYDOWN &&
                   e.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        } else if (e.type == SDL_WINDOWEVENT &&
                   (e.window.event == SDL_WINDOWEVENT_EXPOSED ||
                    e.window.event == SDL_WINDOWEVENT_SHOWN ||
                    e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
          // The window surface may have been invalidated; redraw.
          render();
        }
      }
      SDL_Delay(16);
    }
  }

  return 0;
}
