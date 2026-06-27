#pragma once

#include <cstdint>

namespace pokedex {

struct Point {
  int x;
  int y;
  bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

struct ImageSize {
  int width;
  int height;
  bool operator==(const ImageSize& o) const {
    return width == o.width && height == o.height;
  }
};

struct Rect {
  Point origin;
  ImageSize size;
  bool operator==(const Rect& o) const {
    return origin == o.origin && size == o.size;
  }
};

struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  bool operator==(const Rgb& o) const {
    return r == o.r && g == o.g && b == o.b;
  }
};

}  // namespace pokedex
