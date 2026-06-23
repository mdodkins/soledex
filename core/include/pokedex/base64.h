#pragma once

#include <string>

namespace pokedex {

// Standard base64 (RFC 4648, '+' '/' alphabet, '=' padding) of `in`.
std::string base64Encode(const std::string& in);

// HTTP Basic credential value for `userPass` ("user:password"), i.e.
// "Basic " + base64(userPass). Returns "" for empty input so callers can
// cheaply skip the Authorization header when no auth is configured.
std::string basicAuthHeader(const std::string& userPass);

}  // namespace pokedex
