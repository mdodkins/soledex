#include "pokedex/tcg.h"

#include <gtest/gtest.h>

namespace {

using pokedex::tcgQueryUrl;
using pokedex::urlEncode;

TEST(TcgTest, UrlEncodeLeavesUnreservedCharsAlone) {
  EXPECT_EQ(urlEncode("abcXYZ09-_.~"), "abcXYZ09-_.~");
}

TEST(TcgTest, UrlEncodeEncodesColonAndSpace) {
  EXPECT_EQ(urlEncode("types:fairy energy"), "types%3Afairy%20energy");
}

TEST(TcgTest, UrlEncodeEncodesWildcard) {
  EXPECT_EQ(urlEncode("heal*"), "heal%2A");
}

TEST(TcgTest, BuildsCardSearchUrl) {
  // select=images keeps the response tiny (only image URLs, not full card data)
  // so the on-device cJSON parse stays well within memory.
  EXPECT_EQ(tcgQueryUrl("types:fairy", 20),
            "https://api.pokemontcg.io/v2/cards?q=types%3Afairy&pageSize=20"
            "&select=images");
}

TEST(TcgTest, BuildsUrlForCompoundQuery) {
  EXPECT_EQ(
      tcgQueryUrl("supertype:energy types:fairy", 12),
      "https://api.pokemontcg.io/v2/cards?q=supertype%3Aenergy%20types%3Afairy"
      "&pageSize=12&select=images");
}

}  // namespace
