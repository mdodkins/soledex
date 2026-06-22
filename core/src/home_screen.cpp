#include "pokedex/home_screen.h"

#include "pokedex/png_info.h"

namespace pokedex {

HomeLayout homeLayout(int dispW, int dispH, ImageSize title, ImageSize hero,
                      ImageSize mic, ImageSize kbd) {
  HomeLayout layout;
  layout.title = Point{(dispW - title.width) / 2, kHomeTopMargin};

  // Hero and mic form a vertical stack centred in the space below the title.
  int below = kHomeTopMargin + title.height;
  int group_height = hero.height + kHomeGap + mic.height;
  int group_top = below + (dispH - below - group_height) / 2;

  layout.hero = Point{(dispW - hero.width) / 2, group_top};
  layout.mic = Point{(dispW - mic.width) / 2,
                     group_top + hero.height + kHomeGap};
  layout.kbd = Point{(dispW - kbd.width) / 2,
                     layout.mic.y + mic.height + kHomeGap};
  return layout;
}

bool renderHome(Display& display, const uint8_t* title_png, std::size_t title_len,
                const uint8_t* hero_png, std::size_t hero_len,
                const uint8_t* mic_png, std::size_t mic_len,
                const uint8_t* kbd_png, std::size_t kbd_len) {
  auto title_size = pngSize(title_png, title_len);
  auto hero_size = pngSize(hero_png, hero_len);
  auto mic_size = pngSize(mic_png, mic_len);
  auto kbd_size = pngSize(kbd_png, kbd_len);
  if (!title_size || !hero_size || !mic_size || !kbd_size) {
    return false;
  }
  HomeLayout layout = homeLayout(display.width(), display.height(), *title_size,
                                 *hero_size, *mic_size, *kbd_size);
  display.clear(Rgb{0, 0, 0});
  display.drawPng(title_png, title_len, layout.title.x, layout.title.y);
  display.drawPng(hero_png, hero_len, layout.hero.x, layout.hero.y);
  display.drawPng(mic_png, mic_len, layout.mic.x, layout.mic.y);
  display.drawPng(kbd_png, kbd_len, layout.kbd.x, layout.kbd.y);
  display.present();
  return true;
}

}  // namespace pokedex
