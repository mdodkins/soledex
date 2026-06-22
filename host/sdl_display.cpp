#include "sdl_display.h"

#include <SDL_image.h>

#include <algorithm>
#include <cstdio>

namespace pokedex {

SdlDisplay::SdlDisplay(int width, int height, bool open_window)
    : width_(width), height_(height), open_window_(open_window) {
  // Logical drawing surface, always at the requested (device) resolution.
  surface_ = SDL_CreateRGBSurfaceWithFormat(0, width_, height_, 32,
                                            SDL_PIXELFORMAT_RGBA32);
  if (!surface_) {
    std::fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
    return;
  }
  SDL_SetSurfaceBlendMode(surface_, SDL_BLENDMODE_NONE);  // opaque copy on scale

  if (!open_window_) {
    return;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return;
  }
  video_inited_ = true;

  // Scale the window down to fit within ~90% of the usable display area, while
  // keeping the logical aspect ratio.
  int win_w = width_;
  int win_h = height_;
  SDL_Rect bounds;
  if (SDL_GetDisplayUsableBounds(0, &bounds) == 0) {
    float scale = std::min(bounds.w * 0.9f / width_, bounds.h * 0.9f / height_);
    if (scale < 1.0f) {
      win_w = static_cast<int>(width_ * scale);
      win_h = static_cast<int>(height_ * scale);
    }
  }

  window_ = SDL_CreateWindow("Pokedex", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, win_w, win_h,
                             SDL_WINDOW_SHOWN);
  if (!window_) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
  }
}

SdlDisplay::~SdlDisplay() {
  if (surface_) {
    SDL_FreeSurface(surface_);
  }
  if (window_) {
    SDL_DestroyWindow(window_);
  }
  if (video_inited_) {
    SDL_Quit();
  }
}

void SdlDisplay::clear(Rgb color) {
  if (!surface_) return;
  SDL_FillRect(surface_, nullptr,
               SDL_MapRGB(surface_->format, color.r, color.g, color.b));
}

void SdlDisplay::drawPng(const uint8_t* data, std::size_t len, int x, int y) {
  if (!surface_) return;
  SDL_RWops* rw = SDL_RWFromConstMem(data, static_cast<int>(len));
  SDL_Surface* img = IMG_Load_RW(rw, 1 /* free rw */);
  if (!img) {
    std::fprintf(stderr, "IMG_Load_RW failed: %s\n", IMG_GetError());
    return;
  }
  SDL_SetSurfaceBlendMode(img, SDL_BLENDMODE_BLEND);
  SDL_Rect dst{x, y, img->w, img->h};
  SDL_BlitSurface(img, nullptr, surface_, &dst);
  SDL_FreeSurface(img);
}

void SdlDisplay::present() {
  if (!open_window_ || !window_ || !surface_) return;
  SDL_Surface* ws = SDL_GetWindowSurface(window_);
  if (!ws) return;
  // Scale the logical surface to fill the (possibly smaller) window.
  SDL_BlitScaled(surface_, nullptr, ws, nullptr);
  SDL_UpdateWindowSurface(window_);
}

bool SdlDisplay::savePng(const std::string& path) const {
  return surface_ && IMG_SavePNG(surface_, path.c_str()) == 0;
}

}  // namespace pokedex
