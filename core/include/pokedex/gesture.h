#pragma once

#include "pokedex/geometry.h"

namespace pokedex {

enum class Swipe { None, Left, Right, Up, Down };

// Classify a drag from `start` to `end` into a swipe direction. Movements whose
// larger axis is below `threshold` px are None; otherwise the dominant axis and
// its sign pick the direction (screen coords: +y is down).
Swipe detectSwipe(Point start, Point end, int threshold);

}  // namespace pokedex
