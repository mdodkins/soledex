#include "pokedex/card_prefetch.h"

namespace pokedex {

std::vector<int> prefetchWindow(int index, int count, int back, int ahead) {
  std::vector<int> out;
  auto push = [&](int i) {
    if (i >= 0 && i < count) out.push_back(i);
  };
  push(index);                                       // current first
  for (int i = 1; i <= ahead; ++i) push(index + i);  // then forward
  for (int i = 1; i <= back; ++i) push(index - i);   // then backward
  return out;
}

}  // namespace pokedex
