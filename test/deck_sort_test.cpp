#include "pokedex/deck_sort.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "pokedex/card.h"

namespace {

using pokedex::Card;
using pokedex::CardCategory;
using pokedex::cardCategory;
using pokedex::sortDeckByType;

Card pokemon(const std::string& name) {
  return Card{"u/" + name, name, "Pokémon", ""};
}
Card evo(const std::string& name, const std::string& evolvesFrom) {
  return Card{"u/" + name, name, "Pokémon", "", evolvesFrom};
}
Card trainer(const std::string& name, const std::string& subtype) {
  return Card{"u/" + name, name, "Trainer", subtype};
}
Card energy(const std::string& name) {
  return Card{"u/" + name, name, "Energy", ""};
}

std::vector<std::string> names(const std::vector<Card>& cards) {
  std::vector<std::string> out;
  for (const auto& c : cards) out.push_back(c.name);
  return out;
}

TEST(DeckSortTest, ClassifiesBySupertypeAndSubtype) {
  EXPECT_EQ(cardCategory(pokemon("Pikachu")), CardCategory::Pokemon);
  EXPECT_EQ(cardCategory(trainer("Professor's Research", "Supporter")),
            CardCategory::Supporter);
  EXPECT_EQ(cardCategory(trainer("Poké Ball", "Item")), CardCategory::Item);
  EXPECT_EQ(cardCategory(trainer("Sky Field", "Stadium")),
            CardCategory::Other);
  EXPECT_EQ(cardCategory(energy("Fire Energy")), CardCategory::Energy);
}

TEST(DeckSortTest, UnknownMetadataIsOther) {
  EXPECT_EQ(cardCategory(Card{"u/legacy", "", "", ""}), CardCategory::Other);
}

TEST(DeckSortTest, GroupsByCategoryInDisplayOrder) {
  std::vector<Card> deck = {
      energy("Fire Energy"),
      trainer("Sky Field", "Stadium"),
      trainer("Poké Ball", "Item"),
      trainer("Professor's Research", "Supporter"),
      pokemon("Pikachu"),
  };
  EXPECT_EQ(names(sortDeckByType(deck)),
            (std::vector<std::string>{"Pikachu", "Professor's Research",
                                      "Poké Ball", "Sky Field", "Fire Energy"}));
}

TEST(DeckSortTest, SortsPokemonAlphabeticallyByName) {
  std::vector<Card> deck = {pokemon("Pikachu"), pokemon("Bulbasaur"),
                            pokemon("Charmander")};
  EXPECT_EQ(names(sortDeckByType(deck)),
            (std::vector<std::string>{"Bulbasaur", "Charmander", "Pikachu"}));
}

TEST(DeckSortTest, AlphabeticalIsCaseInsensitive) {
  std::vector<Card> deck = {pokemon("pidgey"), pokemon("Arbok"),
                            pokemon("Zubat"), pokemon("abra")};
  EXPECT_EQ(names(sortDeckByType(deck)),
            (std::vector<std::string>{"abra", "Arbok", "pidgey", "Zubat"}));
}

TEST(DeckSortTest, KeepsEvolutionLineTogetherInOrder) {
  // Shuffled line plus an unrelated basic. The line stays contiguous and in
  // evolution order; chains are ordered alphabetically by their basic, so the
  // Bulbasaur line (root "Bulbasaur") comes before Charmander.
  std::vector<Card> deck = {
      evo("Venusaur", "Ivysaur"),
      pokemon("Charmander"),
      pokemon("Bulbasaur"),
      evo("Ivysaur", "Bulbasaur"),
  };
  EXPECT_EQ(names(sortDeckByType(deck)),
            (std::vector<std::string>{"Bulbasaur", "Ivysaur", "Venusaur",
                                      "Charmander"}));
}

TEST(DeckSortTest, AnchorsLineOnEarliestStagePresentWhenBasicMissing) {
  // Bulbasaur is not in the deck, so Ivysaur anchors the line.
  std::vector<Card> deck = {evo("Venusaur", "Ivysaur"),
                            evo("Ivysaur", "Bulbasaur")};
  EXPECT_EQ(names(sortDeckByType(deck)),
            (std::vector<std::string>{"Ivysaur", "Venusaur"}));
}

TEST(DeckSortTest, BranchingEvolutionsFollowBasicAlphabetically) {
  std::vector<Card> deck = {evo("Vaporeon", "Eevee"), evo("Jolteon", "Eevee"),
                            pokemon("Eevee"), evo("Flareon", "Eevee")};
  EXPECT_EQ(names(sortDeckByType(deck)),
            (std::vector<std::string>{"Eevee", "Flareon", "Jolteon",
                                      "Vaporeon"}));
}

}  // namespace
