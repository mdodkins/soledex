#pragma once

#include "pokedex/geometry.h"

namespace pokedex {

// True if point p lies within the rectangle at `origin` of the given `size`.
// Top/left edges are inclusive, bottom/right edges exclusive.
bool contains(Point origin, ImageSize size, Point p);

}  // namespace pokedex
