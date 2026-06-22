#include "pokedex/png_info.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "png_test_util.h"

namespace {

using pokedex_test::PngHeader;

TEST(PngInfoTest, ReadsWidthAndHeightFromIhdr) {
  // Distinct values so a width/height swap would be caught.
  auto bytes = PngHeader(16, 8);

  auto size = pokedex::pngSize(bytes.data(), bytes.size());

  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(size->width, 16);
  EXPECT_EQ(size->height, 8);
}

TEST(PngInfoTest, ReturnsNulloptWhenBufferTooShort) {
  // Only the 8-byte signature — no room for an IHDR width/height.
  std::vector<uint8_t> bytes = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

  auto size = pokedex::pngSize(bytes.data(), bytes.size());

  EXPECT_FALSE(size.has_value());
}

}  // namespace
