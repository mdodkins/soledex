#include "pokedex/card_browser.h"

#include <gtest/gtest.h>

namespace {

using pokedex::BrowseAction;
using pokedex::CardBrowser;
using pokedex::Swipe;

TEST(CardBrowserTest, StartsAtFirstCard) {
  CardBrowser b(3);
  EXPECT_EQ(b.index(), 0);
  EXPECT_EQ(b.count(), 3);
}

TEST(CardBrowserTest, SwipeLeftAdvancesToNextCard) {
  CardBrowser b(3);
  EXPECT_EQ(b.apply(Swipe::Left), BrowseAction::NextCard);
  EXPECT_EQ(b.index(), 1);
}

TEST(CardBrowserTest, SwipeLeftClampsAtLastCard) {
  CardBrowser b(2);
  b.apply(Swipe::Left);  // -> 1 (last)
  EXPECT_EQ(b.apply(Swipe::Left), BrowseAction::None);
  EXPECT_EQ(b.index(), 1);
}

TEST(CardBrowserTest, SwipeRightGoesToPreviousCard) {
  CardBrowser b(3);
  b.apply(Swipe::Left);  // -> 1
  EXPECT_EQ(b.apply(Swipe::Right), BrowseAction::PrevCard);
  EXPECT_EQ(b.index(), 0);
}

TEST(CardBrowserTest, SwipeRightClampsAtFirstCard) {
  CardBrowser b(3);
  EXPECT_EQ(b.apply(Swipe::Right), BrowseAction::None);
  EXPECT_EQ(b.index(), 0);
}

TEST(CardBrowserTest, SwipeUpGoesHomeWithoutMovingIndex) {
  CardBrowser b(3);
  b.apply(Swipe::Left);  // -> 1
  EXPECT_EQ(b.apply(Swipe::Up), BrowseAction::GoHome);
  EXPECT_EQ(b.index(), 1);
}

TEST(CardBrowserTest, SwipeDownDoesNothing) {
  CardBrowser b(3);
  EXPECT_EQ(b.apply(Swipe::Down), BrowseAction::None);
  EXPECT_EQ(b.index(), 0);
}

}  // namespace
