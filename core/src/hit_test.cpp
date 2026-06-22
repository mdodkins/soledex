#include "pokedex/hit_test.h"

namespace pokedex {

bool contains(Point origin, ImageSize size, Point p) {
  return p.x >= origin.x && p.x < origin.x + size.width && p.y >= origin.y &&
         p.y < origin.y + size.height;
}

}  // namespace pokedex
