#include "pokedex/battery.h"

#include <gtest/gtest.h>

namespace {

TEST(BatteryTest, FullChargeFillsEveryBar) {
  EXPECT_EQ(pokedex::batteryBars(100, 4), 4);
}

TEST(BatteryTest, EmptyBatteryFillsNoBars) {
  EXPECT_EQ(pokedex::batteryBars(0, 4), 0);
}

TEST(BatteryTest, HalfChargeFillsHalfTheBars) {
  EXPECT_EQ(pokedex::batteryBars(50, 4), 2);
}

TEST(BatteryTest, SliverOfChargeLightsOneBar) {
  EXPECT_EQ(pokedex::batteryBars(1, 4), 1);
}

TEST(BatteryTest, OutOfRangeLevelsClampToBarRange) {
  // getBatteryLevel() can report a junk/error value; never under- or overflow.
  EXPECT_EQ(pokedex::batteryBars(-100, 4), 0);
  EXPECT_EQ(pokedex::batteryBars(150, 4), 4);
}

TEST(BatteryTest, NegativeCurrentMeansCharging) {
  // On the Tab5 the shunt reads negative when current flows into the battery.
  EXPECT_TRUE(pokedex::batteryCharging(-200));
}

TEST(BatteryTest, PositiveCurrentMeansNotCharging) {
  // Discharging: the device drains the pack, so current flows out (positive).
  EXPECT_FALSE(pokedex::batteryCharging(200));
}

TEST(BatteryTest, NearZeroCurrentIsNotCharging) {
  EXPECT_FALSE(pokedex::batteryCharging(0));
  EXPECT_FALSE(pokedex::batteryCharging(-20));  // magnitude below the threshold
}

TEST(BatteryTest, IconSitsInTopRightCorner) {
  // 720-wide screen: x = 720 - margin(16) - width(72) = 632, y = margin(16).
  EXPECT_EQ(pokedex::batteryIconOrigin(720), (pokedex::Point{632, 16}));
}

}  // namespace
