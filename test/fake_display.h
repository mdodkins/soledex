#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "pokedex/display.h"

namespace pokedex_test {

// A Display that records every call instead of drawing, so render logic can be
// asserted on in unit tests without a window or hardware.
class FakeDisplay : public pokedex::Display {
 public:
  struct DrawCall {
    const uint8_t* data;
    std::size_t len;
    int x;
    int y;
  };

  FakeDisplay(int width, int height) : width_(width), height_(height) {}

  int width() const override { return width_; }
  int height() const override { return height_; }
  void clear(pokedex::Rgb color) override { clears.push_back(color); }
  void drawPng(const uint8_t* data, std::size_t len, int x, int y) override {
    draws.push_back({data, len, x, y});
  }
  void present() override { ++present_count; }

  std::vector<pokedex::Rgb> clears;
  std::vector<DrawCall> draws;
  int present_count = 0;

 private:
  int width_;
  int height_;
};

}  // namespace pokedex_test
