#include "pokedex/tcg.h"

namespace pokedex {

namespace {
bool isUnreserved(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
char hexDigit(int v) { return v < 10 ? char('0' + v) : char('A' + v - 10); }
}  // namespace

std::string urlEncode(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (unsigned char c : value) {
    if (isUnreserved(static_cast<char>(c))) {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hexDigit(c >> 4));
      out.push_back(hexDigit(c & 0xF));
    }
  }
  return out;
}

std::string tcgQueryUrl(const std::string& query, int pageSize) {
  // select=images trims the response to just image URLs (not full card data),
  // keeping it small enough to download + parse on-device without exhausting RAM.
  return "https://api.pokemontcg.io/v2/cards?q=" + urlEncode(query) +
         "&pageSize=" + std::to_string(pageSize) + "&select=images";
}

}  // namespace pokedex
