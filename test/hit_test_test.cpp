#include "pokedex/hit_test.h"

#include <gtest/gtest.h>

namespace {

using pokedex::contains;
using pokedex::ImageSize;
using pokedex::Point;

// A 100x80 rectangle whose top-left is at (10, 20).
constexpr Point kOrigin{10, 20};
constexpr ImageSize kSize{100, 80};

TEST(HitTestTest, PointInsideIsContained) {
  EXPECT_TRUE(contains(kOrigin, kSize, Point{50, 50}));
}

TEST(HitTestTest, PointLeftOfRectIsNotContained) {
  EXPECT_FALSE(contains(kOrigin, kSize, Point{5, 50}));
}

TEST(HitTestTest, PointBelowRectIsNotContained) {
  EXPECT_FALSE(contains(kOrigin, kSize, Point{50, 200}));
}

TEST(HitTestTest, TopLeftCornerIsInclusive) {
  EXPECT_TRUE(contains(kOrigin, kSize, Point{10, 20}));
}

TEST(HitTestTest, BottomRightCornerIsExclusive) {
  // origin + size = (110, 100); that exact point is outside.
  EXPECT_FALSE(contains(kOrigin, kSize, Point{110, 100}));
}

}  // namespace
