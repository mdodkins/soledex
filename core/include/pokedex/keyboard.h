#pragma once

#include <string>
#include <vector>

#include "pokedex/geometry.h"

namespace pokedex {

enum class KeyKind { Char, Space, Backspace, Go };

struct Key {
  KeyKind kind;
  char ch;       // the letter, when kind == Char (0 otherwise)
  Point origin;  // top-left of the key's rectangle
  ImageSize size;
};

struct KeyboardLayout {
  std::vector<Key> keys;  // QWERTY letters, then Backspace, then Go
};

// Apply a key press to the current text buffer:
//   Char      -> append the character
//   Backspace -> drop the last character (no-op when empty)
//   Go        -> leave the text unchanged
std::string applyKey(const std::string& text, const Key& key);

// Lay out a letters-only QWERTY keyboard (no digits) with a Backspace key on the
// bottom letter row and a full-width Go button beneath, filling `width` and
// occupying the bottom of a `height`-tall screen.
KeyboardLayout keyboardLayout(int width, int height);

// The key whose rectangle contains p, or nullptr if none.
const Key* keyAt(const KeyboardLayout& kb, Point p);

}  // namespace pokedex
