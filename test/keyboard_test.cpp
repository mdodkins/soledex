#include "pokedex/keyboard.h"

#include <string>

#include <gtest/gtest.h>

namespace {

using pokedex::applyKey;
using pokedex::Key;
using pokedex::KeyboardLayout;
using pokedex::keyAt;
using pokedex::keyboardLayout;
using pokedex::KeyKind;
using pokedex::Point;

// The centre point of a key's rectangle.
Point center(const Key& k) {
  return {k.origin.x + k.size.width / 2, k.origin.y + k.size.height / 2};
}

// The letters of all Char keys, in layout order.
std::string letters(const KeyboardLayout& kb) {
  std::string s;
  for (const auto& k : kb.keys) {
    if (k.kind == KeyKind::Char) s += k.ch;
  }
  return s;
}

int countKind(const KeyboardLayout& kb, KeyKind kind) {
  int n = 0;
  for (const auto& k : kb.keys) {
    if (k.kind == kind) ++n;
  }
  return n;
}

Key charKey(char c) { return Key{KeyKind::Char, c, {0, 0}, {0, 0}}; }
Key backspaceKey() { return Key{KeyKind::Backspace, 0, {0, 0}, {0, 0}}; }
Key goKey() { return Key{KeyKind::Go, 0, {0, 0}, {0, 0}}; }
Key spaceKey() { return Key{KeyKind::Space, ' ', {0, 0}, {0, 0}}; }

TEST(KeyboardTest, CharKeyAppendsCharacter) {
  EXPECT_EQ(applyKey("pik", charKey('a')), "pika");
}

TEST(KeyboardTest, BackspaceRemovesLastCharacter) {
  EXPECT_EQ(applyKey("pika", backspaceKey()), "pik");
}

TEST(KeyboardTest, BackspaceOnEmptyTextIsNoOp) {
  EXPECT_EQ(applyKey("", backspaceKey()), "");
}

TEST(KeyboardTest, GoLeavesTextUnchanged) {
  EXPECT_EQ(applyKey("pika", goKey()), "pika");
}

TEST(KeyboardTest, SpaceKeyAppendsASpace) {
  EXPECT_EQ(applyKey("dark", spaceKey()), "dark ");
}

TEST(KeyboardTest, LayoutHasTwentySixLettersInQwertyOrder) {
  EXPECT_EQ(letters(keyboardLayout(720, 1280)), "qwertyuiopasdfghjklzxcvbnm");
}

TEST(KeyboardTest, LayoutHasOneBackspaceKey) {
  EXPECT_EQ(countKind(keyboardLayout(720, 1280), KeyKind::Backspace), 1);
}

TEST(KeyboardTest, LayoutHasOneGoKey) {
  EXPECT_EQ(countKind(keyboardLayout(720, 1280), KeyKind::Go), 1);
}

TEST(KeyboardTest, LayoutHasOneSpaceKey) {
  EXPECT_EQ(countKind(keyboardLayout(720, 1280), KeyKind::Space), 1);
}

TEST(KeyboardTest, KeyAtReturnsTheKeyContainingThePoint) {
  auto kb = keyboardLayout(720, 1280);
  const Key& q = kb.keys.front();  // 'q', top-left letter
  const Key* hit = keyAt(kb, center(q));
  ASSERT_NE(hit, nullptr);
  EXPECT_EQ(hit->kind, KeyKind::Char);
  EXPECT_EQ(hit->ch, 'q');
}

TEST(KeyboardTest, KeyAtReturnsNullForPointAboveKeyboard) {
  auto kb = keyboardLayout(720, 1280);
  EXPECT_EQ(keyAt(kb, Point{10, 10}), nullptr);  // text-box region, no key
}

TEST(KeyboardTest, GoButtonSitsBelowEveryLetterKey) {
  auto kb = keyboardLayout(720, 1280);
  int letters_bottom = 0;
  const pokedex::Key* go = nullptr;
  for (const auto& k : kb.keys) {
    if (k.kind == KeyKind::Go) go = &k;
    else letters_bottom = std::max(letters_bottom, k.origin.y + k.size.height);
  }
  ASSERT_NE(go, nullptr);
  EXPECT_GE(go->origin.y, letters_bottom);
}

TEST(KeyboardTest, EveryKeyIsANonEmptyRectWithinScreen) {
  const int w = 720, h = 1280;
  for (const auto& k : keyboardLayout(w, h).keys) {
    EXPECT_GT(k.size.width, 0);
    EXPECT_GT(k.size.height, 0);
    EXPECT_GE(k.origin.x, 0);
    EXPECT_GE(k.origin.y, 0);
    EXPECT_LE(k.origin.x + k.size.width, w);
    EXPECT_LE(k.origin.y + k.size.height, h);
  }
}

}  // namespace
