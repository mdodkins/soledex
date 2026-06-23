#include "pokedex/base64.h"

namespace pokedex {

namespace {
constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}  // namespace

std::string base64Encode(const std::string& in) {
  std::string out;
  out.reserve((in.size() + 2) / 3 * 4);

  std::size_t i = 0;
  for (; i + 3 <= in.size(); i += 3) {
    const auto b0 = static_cast<unsigned char>(in[i]);
    const auto b1 = static_cast<unsigned char>(in[i + 1]);
    const auto b2 = static_cast<unsigned char>(in[i + 2]);
    out += kAlphabet[b0 >> 2];
    out += kAlphabet[((b0 & 0x03) << 4) | (b1 >> 4)];
    out += kAlphabet[((b1 & 0x0F) << 2) | (b2 >> 6)];
    out += kAlphabet[b2 & 0x3F];
  }

  const std::size_t rem = in.size() - i;
  if (rem == 1) {
    const auto b0 = static_cast<unsigned char>(in[i]);
    out += kAlphabet[b0 >> 2];
    out += kAlphabet[(b0 & 0x03) << 4];
    out += "==";
  } else if (rem == 2) {
    const auto b0 = static_cast<unsigned char>(in[i]);
    const auto b1 = static_cast<unsigned char>(in[i + 1]);
    out += kAlphabet[b0 >> 2];
    out += kAlphabet[((b0 & 0x03) << 4) | (b1 >> 4)];
    out += kAlphabet[(b1 & 0x0F) << 2];
    out += '=';
  }
  return out;
}

std::string basicAuthHeader(const std::string& userPass) {
  if (userPass.empty()) return "";
  return "Basic " + base64Encode(userPass);
}

}  // namespace pokedex
