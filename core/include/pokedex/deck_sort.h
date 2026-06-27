#pragma once

#include <vector>

#include "pokedex/card.h"

namespace pokedex {

// Card categories in the order the deck is grouped when viewing it: Pokémon
// first, then Trainer cards split into Supporter / Item / Other (Stadiums,
// Tools, ...), then Energy last. Cards with no metadata (legacy decks) fall
// into Other. The enum values are the sort rank.
enum class CardCategory { Pokemon = 0, Supporter, Item, Other, Energy };

// Classify a card by its supertype/subtype.
CardCategory cardCategory(const Card& card);

// Sort a deck for the "View deck" screen: grouped by category in the order
// above, and alphabetically by name (case-insensitive) within each group.
std::vector<Card> sortDeckByType(const std::vector<Card>& cards);

}  // namespace pokedex
