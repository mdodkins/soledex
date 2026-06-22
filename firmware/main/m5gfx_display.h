#pragma once

#include <M5Unified.h>

#include <cstddef>
#include <cstdint>

#include "pokedex/display.h"

namespace pokedex {

// Device implementation of Display, backed by M5Unified's M5.Display (the Tab5
// MIPI-DSI panel). M5.begin() must have been called first (it powers the panel).
class M5GfxDisplay : public Display {
 public:
  // Set portrait orientation and cache the panel dimensions.
  bool begin();

  int width() const override { return width_; }
  int height() const override { return height_; }
  void clear(Rgb color) override;
  void drawPng(const uint8_t* data, std::size_t len, int x, int y) override;
  void present() override {}  // M5.Display draws straight to the panel.

 private:
  int width_ = 0;
  int height_ = 0;
};

}  // namespace pokedex
