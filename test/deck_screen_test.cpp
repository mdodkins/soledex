#include "pokedex/deck_screen.h"

#include <gtest/gtest.h>

#include "pokedex/deck.h"

namespace {

using pokedex::Deck;

TEST(DeckScreenTest, AddButtonLabelIsPlusWhenCardNotInDeck) {
  Deck deck;
  EXPECT_STREQ(pokedex::addButtonLabel(deck, "https://img/pikachu.png"), "+");
}

TEST(DeckScreenTest, AddButtonLabelIsAddedWhenCardInDeck) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  EXPECT_STREQ(pokedex::addButtonLabel(deck, "https://img/pikachu.png"),
               "Added");
}

TEST(DeckScreenTest, AddButtonSitsInTopRightCorner) {
  // 720-wide portrait screen; button is kDeckButtonSize, inset by the margin.
  // x = 720 - margin(24) - width(160) = 536, y = margin(24).
  EXPECT_EQ(pokedex::addButtonOrigin(720), (pokedex::Point{536, 24}));
}

TEST(DeckScreenTest, RemoveButtonSitsInTopLeftCorner) {
  EXPECT_EQ(pokedex::removeButtonOrigin(), (pokedex::Point{24, 24}));
}

}  // namespace
