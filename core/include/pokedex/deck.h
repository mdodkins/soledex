#pragma once

#include <string>
#include <vector>

namespace pokedex {

// The (single) deck the user is building. Cards are identified by their image
// URL — the same string the results browser already carries around. Membership
// is a set: adding a card already present is a no-op, so the "+" button can be
// pressed repeatedly without duplicating. Insertion order is preserved so the
// deck browses in the order cards were added.
class Deck {
 public:
  bool contains(const std::string& card) const;
  void add(const std::string& card);     // idempotent
  void remove(const std::string& card);  // no-op if absent
  int size() const;
  const std::vector<std::string>& cards() const;

  // Persistence: serialize to a flat string (one card per line) for storage in
  // NVS flash, and rebuild from it. Round-trips exactly.
  std::string serialize() const;
  static Deck deserialize(const std::string& blob);

 private:
  std::vector<std::string> cards_;
};

}  // namespace pokedex
