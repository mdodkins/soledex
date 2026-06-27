#include "pokedex/deck.h"

#include <gtest/gtest.h>

#include "pokedex/card.h"

namespace {

using pokedex::Card;
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

TEST(DeckTest, AddCardStoresItsMetadataRecord) {
  Deck deck;
  deck.add(Card{"https://img/pikachu.png", "Pikachu", "Pokémon", ""});

  ASSERT_EQ(deck.records().size(), 1u);
  EXPECT_EQ(deck.records()[0],
            (Card{"https://img/pikachu.png", "Pikachu", "Pokémon", ""}));
}

TEST(DeckTest, UpdateFillsInMetadataForExistingCard) {
  Deck deck;
  deck.add("https://img/pikachu.png");  // legacy: no metadata
  deck.update(Card{"https://img/pikachu.png", "Pikachu", "Pokémon", ""});

  ASSERT_EQ(deck.records().size(), 1u);
  EXPECT_EQ(deck.records()[0],
            (Card{"https://img/pikachu.png", "Pikachu", "Pokémon", ""}));
}

TEST(DeckTest, UpdateReportsWhetherItMatched) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  EXPECT_TRUE(deck.update(
      Card{"https://img/pikachu.png", "Pikachu", "Pokémon", "", ""}));
  EXPECT_FALSE(
      deck.update(Card{"https://img/eevee.png", "Eevee", "Pokémon", "", ""}));
}

TEST(DeckTest, UpdateIsNoOpForAbsentCard) {
  Deck deck;
  deck.add("https://img/pikachu.png");
  deck.update(Card{"https://img/eevee.png", "Eevee", "Pokémon", ""});

  EXPECT_EQ(deck.size(), 1);
  EXPECT_EQ(deck.records()[0], (Card{"https://img/pikachu.png", "", "", ""}));
}

TEST(DeckTest, RoundTripPreservesCardMetadata) {
  Deck deck;
  deck.add(Card{"https://img/pikachu.png", "Pikachu", "Pokémon", ""});
  deck.add(Card{"https://img/sage.png", "Professor's Research", "Trainer",
                "Supporter"});

  Deck restored = Deck::deserialize(deck.serialize());

  ASSERT_EQ(restored.records().size(), 2u);
  EXPECT_EQ(restored.records()[0],
            (Card{"https://img/pikachu.png", "Pikachu", "Pokémon", ""}));
  EXPECT_EQ(restored.records()[1],
            (Card{"https://img/sage.png", "Professor's Research", "Trainer",
                  "Supporter"}));
}

TEST(DeckTest, RoundTripPreservesEvolvesFrom) {
  Deck deck;
  deck.add(Card{"https://img/ivysaur.png", "Ivysaur", "Pokémon", "Stage 1",
                "Bulbasaur"});

  Deck restored = Deck::deserialize(deck.serialize());

  ASSERT_EQ(restored.records().size(), 1u);
  EXPECT_EQ(restored.records()[0],
            (Card{"https://img/ivysaur.png", "Ivysaur", "Pokémon", "Stage 1",
                  "Bulbasaur"}));
}

TEST(DeckTest, DeserializeReadsLegacyBareUrlLines) {
  // Decks saved before metadata existed are one bare URL per line.
  Deck restored = Deck::deserialize(
      "https://img/pikachu.png\nhttps://img/eevee.png\n");

  ASSERT_EQ(restored.records().size(), 2u);
  EXPECT_EQ(restored.records()[0], (Card{"https://img/pikachu.png", "", "", ""}));
  EXPECT_EQ(restored.records()[1], (Card{"https://img/eevee.png", "", "", ""}));
}

}  // namespace
