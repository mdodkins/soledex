#pragma once

#include <SDL.h>

#include <string>

#include "pokedex/display.h"

namespace pokedex {

// SDL2 implementation of Display. All drawing happens on a logical surface of
// the requested size (e.g. 720x1280, matching the Tab5). With open_window=true
// the result is shown in a window scaled to fit the host display; the logical
// resolution is unchanged, so layout/rendering behave exactly as on device.
class SdlDisplay : public Display {
 public:
  SdlDisplay(int width, int height, bool open_window);
  ~SdlDisplay() override;

  bool ok() const { return surface_ != nullptr; }

  int width() const override { return width_; }
  int height() const override { return height_; }
  void clear(Rgb color) override;
  void drawPng(const uint8_t* data, std::size_t len, int x, int y) override;
  void present() override;

  // Write the logical surface (full resolution) to a PNG file.
  bool savePng(const std::string& path) const;

 private:
  int width_;
  int height_;
  bool open_window_;
  bool video_inited_ = false;
  SDL_Window* window_ = nullptr;
  SDL_Surface* surface_ = nullptr;  // logical drawing surface (width_ x height_)
};

}  // namespace pokedex
