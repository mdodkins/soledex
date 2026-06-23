#pragma once

#include <cstddef>
#include <cstdint>

#include "pokedex/display.h"
#include "pokedex/geometry.h"

namespace pokedex {

// Top margin (px) between the top of the screen and the title.
inline constexpr int kHomeTopMargin = 60;
// Vertical gap (px) between the centre image and the mic button below it.
inline constexpr int kHomeGap = 20;
// The "View deck" button is a text button (no PNG), so its size is fixed here
// rather than read from an image.
inline constexpr ImageSize kViewDeckButtonSize{280, 90};

struct HomeLayout {
  Point title;
  Point deck;  // the "View deck" button, above the hero
  Point hero;  // the centre image (e.g. Jolteon)
  Point mic;
  Point kbd;   // the keyboard button, below the mic
};

// Lay out the home screen, all horizontally centred: the title sits at the top
// margin; the hero image and the mic button form a vertical stack (hero above
// mic, separated by kHomeGap) centred in the space beneath the title. The
// keyboard button sits centred a kHomeGap below the mic.
HomeLayout homeLayout(int dispW, int dispH, ImageSize title, ImageSize hero,
                      ImageSize mic, ImageSize kbd);

// Draw the home screen (clear → title → hero → mic → kbd → present). Returns
// false if any PNG's size can't be read.
bool renderHome(Display& display, const uint8_t* title_png, std::size_t title_len,
                const uint8_t* hero_png, std::size_t hero_len,
                const uint8_t* mic_png, std::size_t mic_len,
                const uint8_t* kbd_png, std::size_t kbd_len);

}  // namespace pokedex
