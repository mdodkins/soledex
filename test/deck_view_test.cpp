#include "pokedex/deck_view.h"

#include <gtest/gtest.h>

#include "pokedex/deck_screen.h"
#include "pokedex/geometry.h"

namespace {

using pokedex::DeckViewMode;
using pokedex::Point;
using pokedex::Rect;

TEST(DeckViewTest, ToggleFlipsBetweenSingleAndGrid) {
  EXPECT_EQ(pokedex::toggle(DeckViewMode::Single), DeckViewMode::Grid);
  EXPECT_EQ(pokedex::toggle(DeckViewMode::Grid), DeckViewMode::Single);
}

TEST(DeckViewTest, ToggleLabelInvitesTheOtherView) {
  EXPECT_STREQ(pokedex::viewToggleLabel(DeckViewMode::Single), "4 cards");
  EXPECT_STREQ(pokedex::viewToggleLabel(DeckViewMode::Grid), "1 card");
}

TEST(DeckViewTest, ToggleButtonSitsInTopRightCorner) {
  // 720-wide screen, kViewToggleButtonSize 220 wide, margin 24:
  // x = 720 - 24 - 220 = 476, y = 24.
  EXPECT_EQ(pokedex::viewToggleButtonOrigin(720), (Point{476, 24}));
}

TEST(DeckViewTest, ToggleButtonIsWiderThanAStandardDeckButton) {
  // "4 cards" / "1 card" needs padding either side of the text.
  EXPECT_GT(pokedex::kViewToggleButtonSize.width, pokedex::kDeckButtonSize.width);
}

TEST(DeckViewTest, GridPageCountIsCeilOfCountOverFour) {
  EXPECT_EQ(pokedex::gridPageCount(0), 0);
  EXPECT_EQ(pokedex::gridPageCount(1), 1);
  EXPECT_EQ(pokedex::gridPageCount(4), 1);
  EXPECT_EQ(pokedex::gridPageCount(5), 2);
  EXPECT_EQ(pokedex::gridPageCount(8), 2);
  EXPECT_EQ(pokedex::gridPageCount(9), 3);
}

TEST(DeckViewTest, CardIndexCombinesPageAndCell) {
  EXPECT_EQ(pokedex::cardIndexAt(0, 0), 0);
  EXPECT_EQ(pokedex::cardIndexAt(0, 3), 3);
  EXPECT_EQ(pokedex::cardIndexAt(1, 0), 4);
  EXPECT_EQ(pokedex::cardIndexAt(2, 1), 9);
}

TEST(DeckViewTest, GridCellsAreTwoByTwoWithGapsAndVerticalMargins) {
  // 720x1280, gap 12, top margin 80 (counter), bottom margin 110 (status strip).
  // cellW=(720-36)/2=342; cellH=(1280-80-110-12)/2=539.
  // Rows at y=80 and y=80+539+12=631. The bottom row ends at 631+539=1170,
  // exactly where the status strip starts (h-110), so they never overlap.
  auto cells = pokedex::gridCells(720, 1280);
  EXPECT_EQ(cells[0], (Rect{{12, 80}, {342, 539}}));
  EXPECT_EQ(cells[1], (Rect{{366, 80}, {342, 539}}));
  EXPECT_EQ(cells[2], (Rect{{12, 631}, {342, 539}}));
  EXPECT_EQ(cells[3], (Rect{{366, 631}, {342, 539}}));
}

TEST(DeckViewTest, GridCellAtFindsTheTappedCell) {
  EXPECT_EQ(pokedex::gridCellAt(720, 1280, Point{20, 100}), 0);
  EXPECT_EQ(pokedex::gridCellAt(720, 1280, Point{400, 100}), 1);
  EXPECT_EQ(pokedex::gridCellAt(720, 1280, Point{20, 700}), 2);
  EXPECT_EQ(pokedex::gridCellAt(720, 1280, Point{400, 700}), 3);
}

TEST(DeckViewTest, GridCellAtReturnsMinusOneInTheGap) {
  // x in the central gap between columns: cell 0 ends at 12+342=354,
  // cell 1 starts at 366, so 360 is between them.
  EXPECT_EQ(pokedex::gridCellAt(720, 1280, Point{360, 100}), -1);
}

TEST(DeckViewTest, GridCellAtReturnsMinusOneAboveTheGrid) {
  // The top margin (y < 80) is reserved for the counter, not a card.
  EXPECT_EQ(pokedex::gridCellAt(720, 1280, Point{20, 40}), -1);
}

}  // namespace
