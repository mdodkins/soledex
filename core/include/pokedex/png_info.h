#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "pokedex/geometry.h"

namespace pokedex {

// Parse the pixel dimensions of an encoded PNG from its IHDR chunk.
// Returns nullopt if the data is not a valid/long-enough PNG header.
std::optional<ImageSize> pngSize(const uint8_t* data, size_t len);

}  // namespace pokedex
