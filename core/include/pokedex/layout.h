#pragma once

#include "pokedex/geometry.h"

namespace pokedex {

// Top-left origin at which an image of (imgW x imgH) should be drawn so that
// it is centred within a display of (dispW x dispH). Integer division floors.
Point centerOrigin(int dispW, int dispH, int imgW, int imgH);

}  // namespace pokedex
