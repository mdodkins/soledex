#include "pokedex/gesture.h"

#include <gtest/gtest.h>

namespace {

using pokedex::detectSwipe;
using pokedex::Point;
using pokedex::Swipe;

constexpr int kThreshold = 50;

TEST(GestureTest, SmallMovementIsNoSwipe) {
  EXPECT_EQ(detectSwipe(Point{100, 100}, Point{120, 110}, kThreshold),
            Swipe::None);
}

TEST(GestureTest, RightwardDragIsRight) {
  EXPECT_EQ(detectSwipe(Point{100, 100}, Point{300, 100}, kThreshold),
            Swipe::Right);
}

TEST(GestureTest, LeftwardDragIsLeft) {
  EXPECT_EQ(detectSwipe(Point{300, 100}, Point{50, 100}, kThreshold),
            Swipe::Left);
}

TEST(GestureTest, UpwardDragIsUp) {
  // +y is down, so moving up means end.y < start.y.
  EXPECT_EQ(detectSwipe(Point{100, 300}, Point{100, 50}, kThreshold),
            Swipe::Up);
}

TEST(GestureTest, DownwardDragIsDown) {
  EXPECT_EQ(detectSwipe(Point{100, 100}, Point{100, 300}, kThreshold),
            Swipe::Down);
}

TEST(GestureTest, DiagonalUsesDominantAxis) {
  // dx=200, dy=-60 -> horizontal dominates -> Right.
  EXPECT_EQ(detectSwipe(Point{0, 100}, Point{200, 40}, kThreshold),
            Swipe::Right);
}

}  // namespace
