#pragma once

#include <cstddef>
#include <cstdint>

#include "pokedex/display.h"

namespace pokedex {

// Clear the display to black and draw the encoded PNG centred on it, then
// present. Returns false (and draws nothing) if the PNG size can't be read.
bool renderCentered(Display& display, const uint8_t* png, std::size_t len);

}  // namespace pokedex
