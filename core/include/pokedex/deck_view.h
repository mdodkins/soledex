#pragma once

#include <array>

#include "pokedex/geometry.h"

namespace pokedex {

// The "View deck" screen shows either one card at a time (Single) or a 2x2 grid
// of four (Grid). A button in the top-right corner toggles between them: it
// reads "4 cards" while in Single view (tap to see four) and "1 card" while in
// Grid view (tap to go back to one).
enum class DeckViewMode { Single, Grid };

DeckViewMode toggle(DeckViewMode mode);
const char* viewToggleLabel(DeckViewMode mode);

// The toggle button sits in the top-right corner. It is wider than the standard
// deck add/remove buttons so "4 cards" / "1 card" has padding either side.
inline constexpr ImageSize kViewToggleButtonSize{220, 96};
Point viewToggleButtonOrigin(int dispW);

// Grid layout: four equal cells in a 2x2 arrangement. A uniform gap separates
// the columns/rows; the top margin leaves room for the "i / N" counter and the
// bottom margin clears the status strip, so neither row collides with them.
// Cells are ordered left-to-right, top-to-bottom (0,1 / 2,3).
inline constexpr int kGridGap = 12;
inline constexpr int kGridCells = 4;
inline constexpr int kGridTopMargin = 80;     // room for the counter
inline constexpr int kGridBottomMargin = 110;  // clears the status strip

std::array<Rect, kGridCells> gridCells(int dispW, int dispH);

// Which grid cell (0..3) a tap lands in, or -1 if it's in the gap/outside.
int gridCellAt(int dispW, int dispH, Point tap);

// Number of grid pages needed for `cardCount` cards (four per page).
int gridPageCount(int cardCount);

// The deck index shown in `cell` (0..3) of page `page`.
int cardIndexAt(int page, int cell);

}  // namespace pokedex
