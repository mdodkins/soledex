#include "pokedex/deck_screen.h"

namespace pokedex {

// Stubs (TDD red).
Point addButtonOrigin(int dispW) {
  return Point{dispW - kDeckButtonMargin - kDeckButtonSize.width,
               kDeckButtonMargin};
}
Point removeButtonOrigin() {
  return Point{kDeckButtonMargin, kDeckButtonMargin};
}
const char* addButtonLabel(const Deck& deck, const std::string& card) {
  return deck.contains(card) ? "Added" : "+";
}

}  // namespace pokedex
