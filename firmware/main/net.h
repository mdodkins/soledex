#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pokedex {

// POST recorded PCM (mono int16 @ sampleRate) to the STT server as a WAV and
// return the transcript. Returns false on any HTTP/parse failure.
bool sttTranscribe(const int16_t* pcm, std::size_t samples, uint32_t sampleRate,
                   std::string& text_out);

// Ask Claude to map a spoken phrase to a pokemontcg.io `q` query string.
bool claudeQuery(const std::string& transcript, std::string& query_out);

// Fetch up to `limit` card image URLs (images.large) for a query.
bool tcgFetchImageUrls(const std::string& query, int limit,
                       std::vector<std::string>& urls_out);

// Plain HTTP(S) GET; collects the body into out. Returns the HTTP status code
// (or -1 on transport failure).
int httpGet(const std::string& url, std::string& out);

}  // namespace pokedex
