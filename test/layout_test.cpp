#include "pokedex/layout.h"

#include <gtest/gtest.h>

namespace {

TEST(LayoutTest, CentersSmallerImageWithinDisplay) {
  auto origin = pokedex::centerOrigin(100, 100, 50, 50);

  EXPECT_EQ(origin, (pokedex::Point{25, 25}));
}

}  // namespace
