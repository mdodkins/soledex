#include "pokedex/png_info.h"

namespace pokedex {

namespace {
int ReadBe32(const uint8_t* p) {
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
}  // namespace

std::optional<ImageSize> pngSize(const uint8_t* data, size_t len) {
  // Width and height are the first two fields of the IHDR chunk, which begins
  // at byte 16 (8-byte signature + 4-byte length + 4-byte "IHDR" tag), so we
  // need at least 24 bytes to read both.
  if (len < 24) {
    return std::nullopt;
  }
  return ImageSize{ReadBe32(data + 16), ReadBe32(data + 20)};
}

}  // namespace pokedex
