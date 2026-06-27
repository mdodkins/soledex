#pragma once

#include <string>
#include <vector>

namespace pokedex {

// Percent-encode a string for use in a URL query value (RFC 3986 unreserved
// characters pass through; everything else becomes %XX).
std::string urlEncode(const std::string& value);

// Build a pokemontcg.io search URL for a Lucene-style query (e.g.
// "types:fairy") requesting up to pageSize cards.
std::string tcgQueryUrl(const std::string& query, int pageSize);

// Recover a pokemontcg.io card id from its image URL, e.g.
// "https://images.pokemontcg.io/xyp/XY04_hires.png" -> "xyp-XY04" and
// "https://images.pokemontcg.io/pop4/9_hires.png" -> "pop4-9". The id is the
// set folder plus the file's number (with any "_hires"/".png" suffix stripped).
// Returns "" if the URL doesn't have the expected ".../<set>/<number>.png" shape.
std::string cardIdFromImageUrl(const std::string& url);

// Build a Lucene `q` value matching any of `ids`, e.g.
// {"xyp-XY04","pop4-9"} -> "id:xyp-XY04 OR id:pop4-9". Empty for no ids.
std::string tcgIdQuery(const std::vector<std::string>& ids);

}  // namespace pokedex
