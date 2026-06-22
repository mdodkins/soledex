#pragma once

#include <cstddef>
#include <cstdint>

#include "pokedex/geometry.h"

namespace pokedex {

// Abstract drawing surface. UI/render logic depends only on this interface,
// so it can run against SdlDisplay (host window / offscreen), M5GfxDisplay
// (the Tab5 panel), or a FakeDisplay (unit tests).
class Display {
 public:
  virtual ~Display() = default;

  virtual int width() const = 0;
  virtual int height() const = 0;

  // Fill the whole surface with a solid colour.
  virtual void clear(Rgb color) = 0;

  // Decode and blit an encoded PNG with its top-left corner at (x, y).
  virtual void drawPng(const uint8_t* data, size_t len, int x, int y) = 0;

  // Flush buffered drawing to the screen.
  virtual void present() = 0;
};

}  // namespace pokedex
