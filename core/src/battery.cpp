#include "pokedex/battery.h"

namespace pokedex {

// Stubs (TDD red).
int batteryBars(int level, int barCount) {
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  // Round up so even a sliver of charge lights the first bar.
  return (level * barCount + 99) / 100;
}
Point batteryIconOrigin(int dispW) {
  return Point{dispW - kBatteryMargin - kBatteryIconSize.width, kBatteryMargin};
}

}  // namespace pokedex
