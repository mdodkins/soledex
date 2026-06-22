#include "pokedex/keyboard.h"

#include "pokedex/hit_test.h"

namespace pokedex {

std::string applyKey(const std::string& text, const Key& key) {
  if (key.kind == KeyKind::Backspace) {
    return text.empty() ? text : text.substr(0, text.size() - 1);
  }
  if (key.kind == KeyKind::Space) {
    return text + ' ';
  }
  if (key.kind == KeyKind::Char) {
    return text + key.ch;
  }
  return text;
}

KeyboardLayout keyboardLayout(int width, int height) {
  // Square keys on a 10-column grid; three letter rows stacked above a
  // full-width Go button at the very bottom of the screen.
  const int cols = 10;
  const int keyW = width / cols;
  const int keyH = keyW;
  const int goH = keyH;
  // Three letter rows + a Space row, stacked above the full-width Go button.
  const int rowsTop = height - goH - 4 * keyH;

  const char* rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  KeyboardLayout kb;
  for (int r = 0; r < 3; ++r) {
    const char* row = rows[r];
    const int n = static_cast<int>(std::char_traits<char>::length(row)) +
                  (r == 2 ? 1 : 0);  // Backspace shares the bottom row
    const int xoff = (width - n * keyW) / 2;
    const int y = rowsTop + r * keyH;
    int col = 0;
    for (const char* c = row; *c; ++c, ++col) {
      kb.keys.push_back(
          Key{KeyKind::Char, *c, {xoff + col * keyW, y}, {keyW, keyH}});
    }
    if (r == 2) {
      kb.keys.push_back(
          Key{KeyKind::Backspace, 0, {xoff + col * keyW, y}, {keyW, keyH}});
    }
  }
  // Wide centred space bar (6 columns) on its own row beneath the letters.
  const int spaceW = 6 * keyW;
  kb.keys.push_back(Key{KeyKind::Space, ' ',
                        {(width - spaceW) / 2, rowsTop + 3 * keyH},
                        {spaceW, keyH}});
  kb.keys.push_back(Key{KeyKind::Go, 0, {0, height - goH}, {width, goH}});
  return kb;
}

const Key* keyAt(const KeyboardLayout& kb, Point p) {
  for (const Key& k : kb.keys) {
    if (contains(k.origin, k.size, p)) return &k;
  }
  return nullptr;
}

}  // namespace pokedex
