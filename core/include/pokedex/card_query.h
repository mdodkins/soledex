#pragma once

#include <string>

namespace pokedex {

// Represents a query against the pokemontcg.io API. `q` is the value passed as
// the `q=` Lucene-style search parameter (e.g. "types:Fairy").
struct CardQuery {
  std::string q;

  bool operator==(const CardQuery& other) const { return q == other.q; }
};

}  // namespace pokedex
