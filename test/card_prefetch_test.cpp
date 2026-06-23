#include "pokedex/card_prefetch.h"

#include <gtest/gtest.h>

namespace {

using pokedex::prefetchWindow;
using Ints = std::vector<int>;

TEST(CardPrefetchTest, MiddleWindowCurrentThenForwardThenBackward) {
  // index 5 of 10, keep 1 behind and 2 ahead: current, forward, then backward.
  EXPECT_EQ(prefetchWindow(5, 10, 1, 2), (Ints{5, 6, 7, 4}));
}

TEST(CardPrefetchTest, OmitsNeighboursBeforeTheFirstCard) {
  // At the start there is nothing behind index 0.
  EXPECT_EQ(prefetchWindow(0, 10, 1, 2), (Ints{0, 1, 2}));
}

TEST(CardPrefetchTest, OmitsNeighboursPastTheLastCard) {
  // At the end there is nothing ahead of index 9.
  EXPECT_EQ(prefetchWindow(9, 10, 1, 2), (Ints{9, 8}));
}

TEST(CardPrefetchTest, SingleCardWindowIsJustThatCard) {
  EXPECT_EQ(prefetchWindow(0, 1, 1, 2), (Ints{0}));
}

}  // namespace
