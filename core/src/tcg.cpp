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

std::string cardIdFromImageUrl(const std::string& url) {
  std::size_t slash = url.rfind('/');
  if (slash == std::string::npos || slash == 0) return "";
  std::string file = url.substr(slash + 1);
  std::size_t prev = url.rfind('/', slash - 1);
  if (prev == std::string::npos) return "";
  std::string set = url.substr(prev + 1, slash - prev - 1);

  // Strip ".png" then a trailing "_hires" to leave the card number.
  const std::string png = ".png";
  if (file.size() >= png.size() &&
      file.compare(file.size() - png.size(), png.size(), png) == 0) {
    file.erase(file.size() - png.size());
  }
  const std::string hires = "_hires";
  if (file.size() >= hires.size() &&
      file.compare(file.size() - hires.size(), hires.size(), hires) == 0) {
    file.erase(file.size() - hires.size());
  }
  if (set.empty() || file.empty()) return "";
  return set + "-" + file;
}

std::string tcgIdQuery(const std::vector<std::string>& ids) {
  std::string q;
  for (const auto& id : ids) {
    if (!q.empty()) q += " OR ";
    q += "id:" + id;
  }
  return q;
}

std::string tcgQueryUrl(const std::string& query, int pageSize) {
  // select trims the response to the image URL plus the type metadata the deck
  // sorts by (name/supertype/subtypes), not full card data — keeping it small
  // enough to download + parse on-device without exhausting RAM.
  return "https://api.pokemontcg.io/v2/cards?q=" + urlEncode(query) +
         "&pageSize=" + std::to_string(pageSize) +
         "&select=images,name,supertype,subtypes,evolvesFrom";
}

}  // namespace pokedex
