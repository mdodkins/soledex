#pragma once

#include "pokedex/geometry.h"

namespace pokedex {

// How many of `barCount` bars to fill for a charge level (0..100). The level is
// clamped to [0, 100]; a non-empty battery always shows at least one bar, and
// each further bar lights as the level crosses an even fraction (so for 4 bars:
// 0%→0, 1..25%→1, 26..50%→2, 51..75%→3, 76..100%→4).
int batteryBars(int level, int barCount);

// Size of the battery icon (body only; the firmware draws the little terminal
// nub just to the right) and its margin from the screen edge.
inline constexpr int kBatteryMargin = 16;
inline constexpr ImageSize kBatteryIconSize{72, 32};

// Top-left origin of the battery icon, tucked into the top-right corner.
Point batteryIconOrigin(int dispW);

}  // namespace pokedex
