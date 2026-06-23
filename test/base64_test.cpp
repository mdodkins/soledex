#include "pokedex/base64.h"

#include <gtest/gtest.h>

namespace {

// RFC 4648 test vectors.
TEST(Base64Test, RfcVectors) {
  EXPECT_EQ(pokedex::base64Encode(""), "");
  EXPECT_EQ(pokedex::base64Encode("f"), "Zg==");
  EXPECT_EQ(pokedex::base64Encode("fo"), "Zm8=");
  EXPECT_EQ(pokedex::base64Encode("foo"), "Zm9v");
  EXPECT_EQ(pokedex::base64Encode("foob"), "Zm9vYg==");
  EXPECT_EQ(pokedex::base64Encode("fooba"), "Zm9vYmE=");
  EXPECT_EQ(pokedex::base64Encode("foobar"), "Zm9vYmFy");
}

// Pad position depends on input length mod 3: 1 -> "==", 2 -> "=", 0 -> none.
TEST(Base64Test, Padding) {
  EXPECT_EQ(pokedex::base64Encode("A").size() % 4, 0u);
  EXPECT_EQ(pokedex::base64Encode("A").substr(2), "==");
  EXPECT_EQ(pokedex::base64Encode("AB").substr(3), "=");
  EXPECT_EQ(pokedex::base64Encode("ABC").find('='), std::string::npos);
}

// High bytes must use the full alphabet incl. '+' and '/'.
TEST(Base64Test, FullAlphabet) {
  EXPECT_EQ(pokedex::base64Encode(std::string("\xff\xff\xff", 3)), "////");
  EXPECT_EQ(pokedex::base64Encode(std::string("\xfb\xff\xbf", 3)), "+/+/");
  EXPECT_EQ(pokedex::base64Encode(std::string("\x00", 1)), "AA==");
}

TEST(Base64Test, BasicAuthHeaderWrapsCreds) {
  EXPECT_EQ(pokedex::basicAuthHeader("user:pass"), "Basic dXNlcjpwYXNz");
  // RFC 7617 canonical example.
  EXPECT_EQ(pokedex::basicAuthHeader("Aladdin:open sesame"),
            "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
}

// Empty creds -> no header, so the firmware/host skip Authorization entirely.
TEST(Base64Test, BasicAuthHeaderEmptyIsBlank) {
  EXPECT_EQ(pokedex::basicAuthHeader(""), "");
}

}  // namespace
