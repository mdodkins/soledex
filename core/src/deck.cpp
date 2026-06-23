#include "pokedex/deck.h"

namespace pokedex {

// Stubs — no behaviour yet (TDD red).
bool Deck::contains(const std::string& card) const {
  for (const auto& c : cards_) {
    if (c == card) return true;
  }
  return false;
}
void Deck::add(const std::string& card) {
  if (!contains(card)) cards_.push_back(card);
}
void Deck::remove(const std::string& card) {
  for (auto it = cards_.begin(); it != cards_.end(); ++it) {
    if (*it == card) {
      cards_.erase(it);
      return;
    }
  }
}
int Deck::size() const { return static_cast<int>(cards_.size()); }
const std::vector<std::string>& Deck::cards() const { return cards_; }
std::string Deck::serialize() const {
  std::string blob;
  for (const auto& c : cards_) {
    blob += c;
    blob += '\n';
  }
  return blob;
}

Deck Deck::deserialize(const std::string& blob) {
  Deck deck;
  std::string line;
  for (char ch : blob) {
    if (ch == '\n') {
      if (!line.empty()) deck.add(line);
      line.clear();
    } else {
      line += ch;
    }
  }
  if (!line.empty()) deck.add(line);  // tolerate a missing trailing newline
  return deck;
}

}  // namespace pokedex
