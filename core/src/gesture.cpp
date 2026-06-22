#include "pokedex/gesture.h"

namespace pokedex {

namespace {
int abs_int(int v) { return v < 0 ? -v : v; }
}  // namespace

Swipe detectSwipe(Point start, Point end, int threshold) {
  int dx = end.x - start.x;
  int dy = end.y - start.y;
  if (abs_int(dx) < threshold && abs_int(dy) < threshold) {
    return Swipe::None;
  }
  if (abs_int(dx) >= abs_int(dy)) {
    return dx > 0 ? Swipe::Right : Swipe::Left;
  }
  return dy > 0 ? Swipe::Down : Swipe::Up;
}

}  // namespace pokedex
