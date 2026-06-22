#include "m5gfx_display.h"

namespace pokedex {

bool M5GfxDisplay::begin() {
  M5.Display.setRotation(0);  // portrait — Pokemon cards are portrait
  width_ = M5.Display.width();
  height_ = M5.Display.height();
  // The Tab5 MIPI-DSI panel is intermittently not detected at boot; the caller
  // reboots on failure (each fresh boot gets an independent chance).
  return width_ > 0 && height_ > 0;
}

void M5GfxDisplay::clear(Rgb color) {
  M5.Display.fillScreen(M5.Display.color888(color.r, color.g, color.b));
}

void M5GfxDisplay::drawPng(const uint8_t* data, std::size_t len, int x, int y) {
  M5.Display.drawPng(data, static_cast<uint32_t>(len), x, y);
}

}  // namespace pokedex
