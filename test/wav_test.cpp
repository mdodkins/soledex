#include "pokedex/wav.h"

#include <gtest/gtest.h>

#include <string>

namespace {

uint32_t le32(const std::array<uint8_t, 44>& h, int i) {
  return h[i] | (h[i + 1] << 8) | (h[i + 2] << 16) | (h[i + 3] << 24);
}
uint16_t le16(const std::array<uint8_t, 44>& h, int i) {
  return h[i] | (h[i + 1] << 8);
}
std::string tag(const std::array<uint8_t, 44>& h, int i) {
  return std::string(h.begin() + i, h.begin() + i + 4);
}

TEST(WavTest, BuildsPcmMono16HeaderFields) {
  auto h = pokedex::wavHeader(/*dataBytes=*/8000, /*sampleRate=*/16000);

  EXPECT_EQ(tag(h, 0), "RIFF");
  EXPECT_EQ(le32(h, 4), 36u + 8000u);  // chunk size = 36 + data
  EXPECT_EQ(tag(h, 8), "WAVE");
  EXPECT_EQ(tag(h, 12), "fmt ");
  EXPECT_EQ(le32(h, 16), 16u);  // PCM fmt chunk size
  EXPECT_EQ(le16(h, 20), 1u);   // audio format = PCM
  EXPECT_EQ(le16(h, 22), 1u);   // mono
  EXPECT_EQ(le32(h, 24), 16000u);                 // sample rate
  EXPECT_EQ(le32(h, 28), 16000u * 2u);            // byte rate = rate*channels*2
  EXPECT_EQ(le16(h, 32), 2u);                     // block align = channels*2
  EXPECT_EQ(le16(h, 34), 16u);                    // bits per sample
  EXPECT_EQ(tag(h, 36), "data");
  EXPECT_EQ(le32(h, 40), 8000u);  // data size
}

}  // namespace
