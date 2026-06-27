#include "pokedex/deck.h"

namespace pokedex {

bool Deck::contains(const std::string& card) const {
  for (const auto& c : cards_) {
    if (c.url == card) return true;
  }
  return false;
}
void Deck::add(const Card& card) {
  if (!contains(card.url)) cards_.push_back(card);
}
void Deck::add(const std::string& url) { add(Card{url, "", "", ""}); }
bool Deck::update(const Card& card) {
  for (auto& c : cards_) {
    if (c.url == card.url) {
      c = card;
      return true;
    }
  }
  return false;
}
void Deck::remove(const std::string& card) {
  for (auto it = cards_.begin(); it != cards_.end(); ++it) {
    if (it->url == card) {
      cards_.erase(it);
      return;
    }
  }
}
int Deck::size() const { return static_cast<int>(cards_.size()); }
std::vector<std::string> Deck::cards() const {
  std::vector<std::string> urls;
  urls.reserve(cards_.size());
  for (const auto& c : cards_) urls.push_back(c.url);
  return urls;
}
const std::vector<Card>& Deck::records() const { return cards_; }
namespace {
// One card per line, fields tab-separated:
// url\tname\tsupertype\tsubtype\tevolvesFrom. URLs never contain tabs or
// newlines, so this round-trips unambiguously. Lines with fewer fields (legacy
// bare URLs, or pre-evolvesFrom decks) leave the trailing fields empty.
Card parseLine(const std::string& line) {
  Card card;
  std::string* fields[] = {&card.url, &card.name, &card.supertype,
                           &card.subtype, &card.evolvesFrom};
  constexpr int kLast = 4;
  int f = 0;
  for (char ch : line) {
    if (ch == '\t' && f < kLast) {
      ++f;
    } else {
      *fields[f] += ch;
    }
  }
  return card;
}
}  // namespace

std::string Deck::serialize() const {
  std::string blob;
  for (const auto& c : cards_) {
    blob += c.url;
    blob += '\t';
    blob += c.name;
    blob += '\t';
    blob += c.supertype;
    blob += '\t';
    blob += c.subtype;
    blob += '\t';
    blob += c.evolvesFrom;
    blob += '\n';
  }
  return blob;
}

Deck Deck::deserialize(const std::string& blob) {
  Deck deck;
  std::string line;
  for (char ch : blob) {
    if (ch == '\n') {
      if (!line.empty()) deck.add(parseLine(line));
      line.clear();
    } else {
      line += ch;
    }
  }
  if (!line.empty()) deck.add(parseLine(line));  // missing trailing newline
  return deck;
}

}  // namespace pokedex
