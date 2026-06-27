#include "pokedex/deck_sort.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace pokedex {

namespace {
// Case-insensitive lexicographic compare so "abra" < "Arbok" < "pidgey".
bool nameLess(const std::string& a, const std::string& b) {
  std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    int ca = std::tolower(static_cast<unsigned char>(a[i]));
    int cb = std::tolower(static_cast<unsigned char>(b[i]));
    if (ca != cb) return ca < cb;
  }
  return a.size() < b.size();
}
bool nameEqual(const std::string& a, const std::string& b) {
  return !nameLess(a, b) && !nameLess(b, a);
}

// Walk a Pokémon up its evolution chain (via evolvesFrom) as far as the deck
// contains the pre-evolution, returning the anchor name (earliest present
// stage) and the number of steps from it. `evoFrom` maps each present Pokémon
// name to its pre-evolution; `present` is the set of Pokémon names in the deck.
std::pair<std::string, int> evolutionAnchor(
    const std::string& name,
    const std::unordered_map<std::string, std::string>& evoFrom,
    const std::unordered_set<std::string>& present) {
  std::string cur = name;
  int depth = 0;
  std::unordered_set<std::string> seen{cur};
  for (;;) {
    auto it = evoFrom.find(cur);
    if (it == evoFrom.end()) break;
    const std::string& from = it->second;
    if (from.empty() || !present.count(from) || seen.count(from)) break;
    cur = from;
    ++depth;
    seen.insert(cur);
  }
  return {cur, depth};
}
}  // namespace

CardCategory cardCategory(const Card& card) {
  if (card.supertype == "Pokémon" || card.supertype == "Pokemon")
    return CardCategory::Pokemon;
  if (card.supertype == "Energy") return CardCategory::Energy;
  if (card.supertype == "Trainer") {
    if (card.subtype == "Supporter") return CardCategory::Supporter;
    if (card.subtype == "Item") return CardCategory::Item;
  }
  return CardCategory::Other;  // Stadiums, Tools, and unknown/legacy cards
}

std::vector<Card> sortDeckByType(const std::vector<Card>& cards) {
  // Index the Pokémon in the deck so we can keep evolution lines together.
  std::unordered_set<std::string> pokemonNames;
  std::unordered_map<std::string, std::string> evoFrom;
  for (const auto& c : cards) {
    if (cardCategory(c) == CardCategory::Pokemon) {
      pokemonNames.insert(c.name);
      evoFrom[c.name] = c.evolvesFrom;
    }
  }

  std::vector<Card> sorted = cards;
  std::stable_sort(
      sorted.begin(), sorted.end(), [&](const Card& a, const Card& b) {
        CardCategory ca = cardCategory(a);
        CardCategory cb = cardCategory(b);
        if (ca != cb) return ca < cb;
        if (ca == CardCategory::Pokemon) {
          // Order by evolution line: anchor name, then stage, then own name so
          // the line is contiguous (Basic, Stage 1, Stage 2) and branches sort
          // alphabetically. Chains are ordered by their anchor's name.
          auto [ra, da] = evolutionAnchor(a.name, evoFrom, pokemonNames);
          auto [rb, db] = evolutionAnchor(b.name, evoFrom, pokemonNames);
          if (!nameEqual(ra, rb)) return nameLess(ra, rb);
          if (da != db) return da < db;
          return nameLess(a.name, b.name);
        }
        return nameLess(a.name, b.name);
      });
  return sorted;
}

}  // namespace pokedex
