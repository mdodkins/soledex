#include "pokedex/deck.h"

#include <gtest/gtest.h>

namespace {

using pokedex::Deck;

TEST(DeckTest, NewDeckIsEmpty) {
  Deck deck;
  EXPECT_EQ(deck.size(), 0);
  EXPECT_FALSE(deck.contains("https://img/pikachu.png"));
}

TEST(DeckTest, AddedCardIsContained) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  EXPECT_TRUE(deck.contains("https://img/pikachu.png"));
}

TEST(DeckTest, SizeReflectsNumberOfCards) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  deck.add("https://img/eevee.png");
  EXPECT_EQ(deck.size(), 2);
}

TEST(DeckTest, AddingSameCardTwiceIsIdempotent) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  deck.add("https://img/pikachu.png");
  EXPECT_EQ(deck.size(), 1);
}

TEST(DeckTest, RemoveDeletesTheCard) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  deck.remove("https://img/pikachu.png");
  EXPECT_FALSE(deck.contains("https://img/pikachu.png"));
  EXPECT_EQ(deck.size(), 0);
}

TEST(DeckTest, RemovingAbsentCardIsNoOp) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  deck.remove("https://img/eevee.png");
  EXPECT_TRUE(deck.contains("https://img/pikachu.png"));
  EXPECT_EQ(deck.size(), 1);
}

TEST(DeckTest, SerializeEmptyDeckIsEmptyString) {
  Deck deck;
  EXPECT_EQ(deck.serialize(), "");
}

TEST(DeckTest, RoundTripPreservesCardsInOrder) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  deck.add("https://img/eevee.png");
  deck.add("https://img/jolteon.png");

  Deck restored = Deck::deserialize(deck.serialize());

  ASSERT_EQ(restored.size(), 3);
  EXPECT_EQ(restored.cards(),
            (std::vector<std::string>{"https://img/pikachu.png",
                                      "https://img/eevee.png",
                                      "https://img/jolteon.png"}));
}

TEST(DeckTest, DeserializeEmptyStringYieldsEmptyDeck) {
  EXPECT_EQ(Deck::deserialize("").size(), 0);
}

}  // namespace
