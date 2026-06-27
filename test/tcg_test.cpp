#include "pokedex/tcg.h"

#include <gtest/gtest.h>

namespace {

using pokedex::cardIdFromImageUrl;
using pokedex::tcgIdQuery;
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
  // select trims the response to the image URL plus the type metadata the deck
  // sorts by (name/supertype/subtypes), keeping the on-device cJSON parse small.
  EXPECT_EQ(tcgQueryUrl("types:fairy", 20),
            "https://api.pokemontcg.io/v2/cards?q=types%3Afairy&pageSize=20"
            "&select=images,name,supertype,subtypes,evolvesFrom");
}

TEST(TcgTest, BuildsUrlForCompoundQuery) {
  EXPECT_EQ(
      tcgQueryUrl("supertype:energy types:fairy", 12),
      "https://api.pokemontcg.io/v2/cards?q=supertype%3Aenergy%20types%3Afairy"
      "&pageSize=12&select=images,name,supertype,subtypes,evolvesFrom");
}

TEST(TcgTest, RecoversCardIdFromImageUrl) {
  EXPECT_EQ(cardIdFromImageUrl(
                "https://images.pokemontcg.io/xyp/XY04_hires.png"),
            "xyp-XY04");
  EXPECT_EQ(cardIdFromImageUrl("https://images.pokemontcg.io/pop4/9_hires.png"),
            "pop4-9");
}

TEST(TcgTest, RecoversCardIdWithoutHiresSuffix) {
  EXPECT_EQ(cardIdFromImageUrl("https://images.pokemontcg.io/base1/4.png"),
            "base1-4");
}

TEST(TcgTest, CardIdIsEmptyForUnparseableUrl) {
  EXPECT_EQ(cardIdFromImageUrl(""), "");
  EXPECT_EQ(cardIdFromImageUrl("notaurl"), "");
}

TEST(TcgTest, BuildsIdQueryFromIds) {
  EXPECT_EQ(tcgIdQuery({"xyp-XY04", "pop4-9"}), "id:xyp-XY04 OR id:pop4-9");
  EXPECT_EQ(tcgIdQuery({"base1-4"}), "id:base1-4");
  EXPECT_EQ(tcgIdQuery({}), "");
}

}  // namespace
