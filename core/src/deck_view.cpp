#include "pokedex/deck_view.h"

#include "pokedex/deck_screen.h"
#include "pokedex/hit_test.h"

namespace pokedex {

DeckViewMode toggle(DeckViewMode mode) {
  return mode == DeckViewMode::Single ? DeckViewMode::Grid
                                      : DeckViewMode::Single;
}

const char* viewToggleLabel(DeckViewMode mode) {
  return mode == DeckViewMode::Single ? "4 cards" : "1 card";
}

Point viewToggleButtonOrigin(int dispW) {
  return Point{dispW - kDeckButtonMargin - kViewToggleButtonSize.width,
               kDeckButtonMargin};
}

std::array<Rect, kGridCells> gridCells(int dispW, int dispH) {
  const int g = kGridGap;
  const int cellW = (dispW - 3 * g) / 2;
  const int cellH = (dispH - kGridTopMargin - kGridBottomMargin - g) / 2;
  const int x0 = g, x1 = 2 * g + cellW;
  const int y0 = kGridTopMargin, y1 = kGridTopMargin + cellH + g;
  const ImageSize s{cellW, cellH};
  return {Rect{{x0, y0}, s}, Rect{{x1, y0}, s}, Rect{{x0, y1}, s},
          Rect{{x1, y1}, s}};
}

int gridCellAt(int dispW, int dispH, Point tap) {
  auto cells = gridCells(dispW, dispH);
  for (int i = 0; i < kGridCells; ++i) {
    if (contains(cells[i].origin, cells[i].size, tap)) return i;
  }
  return -1;
}

int gridPageCount(int cardCount) {
  return (cardCount + kGridCells - 1) / kGridCells;
}

int cardIndexAt(int page, int cell) { return page * kGridCells + cell; }

}  // namespace pokedex
