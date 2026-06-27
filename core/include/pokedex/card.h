#pragma once

#include <string>

namespace pokedex {

// A card the user can browse and collect. The image URL is the identity (the
// same string the results browser carries around); name/supertype/subtype are
// the pokemontcg.io metadata captured when the card is added so the deck can be
// sorted by type. Legacy decks saved before metadata existed deserialize with
// empty name/supertype/subtype.
struct Card {
  std::string url;
  std::string name;
  std::string supertype;    // "Pokémon", "Trainer", "Energy"
  std::string subtype;      // for Trainers: "Supporter", "Item", "Stadium", ...
  std::string evolvesFrom;  // for Pokémon: the pre-evolution's name, else empty

  bool operator==(const Card& o) const {
    return url == o.url && name == o.name && supertype == o.supertype &&
           subtype == o.subtype && evolvesFrom == o.evolvesFrom;
  }
};

}  // namespace pokedex
