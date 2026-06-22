#pragma once

#include "pokedex/gesture.h"

namespace pokedex {

enum class BrowseAction { None, NextCard, PrevCard, GoHome };

// Tracks which card in a result set is showing and turns swipes into actions:
// Left = next card, Right = previous card (both clamped to the ends),
// Up = go back to the home/search screen. Down is ignored.
class CardBrowser {
 public:
  explicit CardBrowser(int count) : count_(count) {}

  int index() const { return index_; }
  int count() const { return count_; }

  // Apply a swipe; updates the index and reports what happened.
  BrowseAction apply(Swipe swipe);

 private:
  int count_;
  int index_ = 0;
};

}  // namespace pokedex
