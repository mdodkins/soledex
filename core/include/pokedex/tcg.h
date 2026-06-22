#pragma once

#include <string>

namespace pokedex {

// Percent-encode a string for use in a URL query value (RFC 3986 unreserved
// characters pass through; everything else becomes %XX).
std::string urlEncode(const std::string& value);

// Build a pokemontcg.io search URL for a Lucene-style query (e.g.
// "types:fairy") requesting up to pageSize cards.
std::string tcgQueryUrl(const std::string& query, int pageSize);

}  // namespace pokedex
