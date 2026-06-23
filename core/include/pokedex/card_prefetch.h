#pragma once

#include <vector>

namespace pokedex {

// How many cards to keep buffered behind / ahead of the one on screen. Browsing
// is mostly forward, so we look further ahead than behind.
inline constexpr int kPrefetchBack = 1;
inline constexpr int kPrefetchAhead = 2;

// The indices that should be resident in the card cache around `index`, for a
// result set of `count` cards: the current card, `back` before it and `ahead`
// after it, clamped to [0, count). Returned in fetch-priority order — the
// current card first, then forward, then backward — so a caller downloads the
// most useful missing card first. Out-of-range neighbours are simply omitted.
std::vector<int> prefetchWindow(int index, int count, int back, int ahead);

}  // namespace pokedex
