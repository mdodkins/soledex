#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pokedex/card.h"

namespace pokedex {

// POST recorded PCM (mono int16 @ sampleRate) to the STT server as a WAV and
// return the transcript. Returns false on any HTTP/parse failure.
bool sttTranscribe(const int16_t* pcm, std::size_t samples, uint32_t sampleRate,
                   std::string& text_out);

// Ask Claude to map a spoken phrase to a pokemontcg.io `q` query string.
bool claudeQuery(const std::string& transcript, std::string& query_out);

// Fetch up to `limit` cards for a query: image URL (images.large) plus the
// name/supertype/subtype metadata the deck sorts by.
bool tcgFetchCards(const std::string& query, int limit,
                   std::vector<Card>& cards_out);

// Plain HTTP(S) GET; collects the body into out. Returns the HTTP status code
// (or -1 on transport failure).
int httpGet(const std::string& url, std::string& out);

}  // namespace pokedex
