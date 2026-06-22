#pragma once

#include <cstdint>
#include <vector>

namespace pokedex_test {

// Builds the leading bytes of a PNG: 8-byte signature, then an IHDR chunk
// whose first two fields are the width and height (big-endian uint32).
inline std::vector<uint8_t> PngHeader(uint32_t width, uint32_t height) {
  std::vector<uint8_t> bytes = {
      0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,  // signature
      0x00, 0x00, 0x00, 0x0D,                         // IHDR length = 13
      'I',  'H', 'D', 'R',                            // chunk type
  };
  auto push_be32 = [&](uint32_t v) {
    bytes.push_back(static_cast<uint8_t>(v >> 24));
    bytes.push_back(static_cast<uint8_t>(v >> 16));
    bytes.push_back(static_cast<uint8_t>(v >> 8));
    bytes.push_back(static_cast<uint8_t>(v));
  };
  push_be32(width);
  push_be32(height);
  bytes.push_back(8);  // bit depth
  bytes.push_back(6);  // colour type (RGBA)
  return bytes;
}

}  // namespace pokedex_test
