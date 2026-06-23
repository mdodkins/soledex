#pragma once

#include <string>

#include "pokedex/deck.h"
#include "pokedex/geometry.h"

namespace pokedex {

// Geometry of the deck-building buttons that overlay the full-screen card image.
// The firmware draws them as rounded-rect text buttons (like the keyboard keys)
// and hit-tests taps against these rectangles with contains().
inline constexpr int kDeckButtonMargin = 24;
inline constexpr ImageSize kDeckButtonSize{160, 96};

// The "+ / Added" button sits in the top-right corner of the card screen.
Point addButtonOrigin(int dispW);

// The "-" button sits in the top-left corner when viewing the deck.
Point removeButtonOrigin();

// Label for the add button: "+" until the card is in the deck, then "Added".
const char* addButtonLabel(const Deck& deck, const std::string& card);

}  // namespace pokedex
