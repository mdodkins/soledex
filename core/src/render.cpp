#include "pokedex/render.h"

#include "pokedex/layout.h"
#include "pokedex/png_info.h"

namespace pokedex {

bool renderCentered(Display& display, const uint8_t* png, std::size_t len) {
  auto size = pngSize(png, len);
  if (!size) {
    return false;
  }
  display.clear(Rgb{0, 0, 0});
  Point origin =
      centerOrigin(display.width(), display.height(), size->width, size->height);
  display.drawPng(png, len, origin.x, origin.y);
  display.present();
  return true;
}

}  // namespace pokedex
