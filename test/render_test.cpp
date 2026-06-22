#include "pokedex/render.h"

#include <gtest/gtest.h>

#include "fake_display.h"
#include "png_test_util.h"

namespace {

TEST(RenderTest, ClearsCentersAndPresents) {
  pokedex_test::FakeDisplay display(1280, 720);
  auto png = pokedex_test::PngHeader(475, 475);

  bool ok = pokedex::renderCentered(display, png.data(), png.size());

  EXPECT_TRUE(ok);
  // Cleared exactly once, to black.
  ASSERT_EQ(display.clears.size(), 1u);
  EXPECT_EQ(display.clears[0], (pokedex::Rgb{0, 0, 0}));
  // Drew the image once, centred: x=(1280-475)/2, y=(720-475)/2.
  ASSERT_EQ(display.draws.size(), 1u);
  EXPECT_EQ(display.draws[0].data, png.data());
  EXPECT_EQ(display.draws[0].len, png.size());
  EXPECT_EQ(display.draws[0].x, 402);
  EXPECT_EQ(display.draws[0].y, 122);
  // Flushed once.
  EXPECT_EQ(display.present_count, 1);
}

}  // namespace
