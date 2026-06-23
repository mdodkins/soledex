#include "pokedex/home_screen.h"

#include <gtest/gtest.h>

#include "fake_display.h"
#include "png_test_util.h"

namespace {

TEST(HomeScreenTest, CentersTitleWithDeckHeroAndMicStackedBelow) {
  // title 700x180, hero 200x200, mic 120x120, kbd 120x120 on 1280x720;
  // topMargin 60, gap 20, deck button 280x90.
  auto layout = pokedex::homeLayout(1280, 720, {700, 180}, {200, 200},
                                    {120, 120}, {120, 120});

  // Title: x=(1280-700)/2=290, y=60.
  EXPECT_EQ(layout.title, (pokedex::Point{290, 60}));
  // Deck + hero + mic form a stack centred below the title:
  //   below=60+180=240; areaH=720-240=480; groupH=90+20+200+20+120=450;
  //   groupTop=240+(480-450)/2=255.
  // Hero: x=(1280-200)/2=540, y=255+90+20=365.
  EXPECT_EQ(layout.hero, (pokedex::Point{540, 365}));
  // Mic: x=(1280-120)/2=580, y=365+200+20=585.
  EXPECT_EQ(layout.mic, (pokedex::Point{580, 585}));
}

TEST(HomeScreenTest, PlacesViewDeckButtonAboveHero) {
  auto layout = pokedex::homeLayout(1280, 720, {700, 180}, {200, 200},
                                    {120, 120}, {120, 120});
  // Deck button (kViewDeckButtonSize 280x90) tops the centred stack:
  //   below=240; areaH=480; groupH=90+20+200+20+120=450;
  //   groupTop=240+(480-450)/2=255.
  // Deck: x=(1280-280)/2=500, y=255.
  EXPECT_EQ(layout.deck, (pokedex::Point{500, 255}));
  // Hero sits a gap below the deck button: y=255+90+20=365.
  EXPECT_EQ(layout.hero, (pokedex::Point{540, 365}));
}

TEST(HomeScreenTest, PlacesKeyboardCentredBelowMic) {
  auto layout = pokedex::homeLayout(1280, 720, {700, 180}, {200, 200},
                                    {120, 120}, {120, 120});
  // Keyboard: x=(1280-120)/2=580, y=mic.y(585)+mic.h(120)+gap(20)=725.
  EXPECT_EQ(layout.kbd, (pokedex::Point{580, 725}));
}

TEST(HomeScreenTest, RendersTitleHeroThenMicAtLayoutPositions) {
  pokedex_test::FakeDisplay display(1280, 720);
  auto title = pokedex_test::PngHeader(700, 180);
  auto hero = pokedex_test::PngHeader(200, 200);
  auto mic = pokedex_test::PngHeader(120, 120);
  auto kbd = pokedex_test::PngHeader(120, 120);

  bool ok = pokedex::renderHome(display, title.data(), title.size(),
                                hero.data(), hero.size(), mic.data(),
                                mic.size(), kbd.data(), kbd.size());

  EXPECT_TRUE(ok);
  ASSERT_EQ(display.clears.size(), 1u);
  EXPECT_EQ(display.clears[0], (pokedex::Rgb{0, 0, 0}));
  // The "View deck" button is a firmware-drawn text button, so renderHome only
  // blits the four PNGs (title, hero, mic, kbd) — it leaves the gap above the
  // hero for the deck button.
  ASSERT_EQ(display.draws.size(), 4u);
  // Title first.
  EXPECT_EQ(display.draws[0].data, title.data());
  EXPECT_EQ(display.draws[0].x, 290);
  EXPECT_EQ(display.draws[0].y, 60);
  // Hero (Jolteon) second, in the middle.
  EXPECT_EQ(display.draws[1].data, hero.data());
  EXPECT_EQ(display.draws[1].x, 540);
  EXPECT_EQ(display.draws[1].y, 365);
  // Mic third, underneath the hero.
  EXPECT_EQ(display.draws[2].data, mic.data());
  EXPECT_EQ(display.draws[2].x, 580);
  EXPECT_EQ(display.draws[2].y, 585);
  // Keyboard last, beneath the mic.
  EXPECT_EQ(display.draws[3].data, kbd.data());
  EXPECT_EQ(display.draws[3].x, 580);
  EXPECT_EQ(display.draws[3].y, 725);
  EXPECT_EQ(display.present_count, 1);
}

}  // namespace
