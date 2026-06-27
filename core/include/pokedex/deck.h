#pragma once

#include <string>
#include <vector>

#include "pokedex/card.h"

namespace pokedex {

// The (single) deck the user is building. Cards are identified by their image
// URL — the same string the results browser already carries around. Membership
// is a set: adding a card already present is a no-op, so the "+" button can be
// pressed repeatedly without duplicating. Insertion order is preserved so the
// deck browses in the order cards were added. Each card also carries the
// pokemontcg.io metadata (name/supertype/subtype) captured at add time so the
// deck can be sorted by type.
class Deck {
 public:
  bool contains(const std::string& card) const;
  void add(const Card& card);            // idempotent (by url)
  void add(const std::string& url);      // convenience: no metadata
  // Fill in metadata for the card with this url (matched by url). Returns true
  // if a card was updated, false if the url isn't in the deck. Used to backfill
  // type metadata for legacy cards.
  bool update(const Card& card);
  void remove(const std::string& card);  // no-op if absent
  int size() const;

  // URLs in insertion order (what the firmware browser works with).
  std::vector<std::string> cards() const;
  // Full records in insertion order (url + metadata), for sorting by type.
  const std::vector<Card>& records() const;

  // Persistence: serialize to a flat string (one card per line) for storage in
  // NVS flash, and rebuild from it. Round-trips exactly. Tolerates legacy
  // bare-URL lines (no metadata) saved before metadata existed.
  std::string serialize() const;
  static Deck deserialize(const std::string& blob);

 private:
  std::vector<Card> cards_;
};

}  // namespace pokedex
