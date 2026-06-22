#include "pokedex/wav.h"

namespace pokedex {

namespace {
void put32(std::array<uint8_t, 44>& h, int i, uint32_t v) {
  h[i] = v & 0xFF;
  h[i + 1] = (v >> 8) & 0xFF;
  h[i + 2] = (v >> 16) & 0xFF;
  h[i + 3] = (v >> 24) & 0xFF;
}
void put16(std::array<uint8_t, 44>& h, int i, uint16_t v) {
  h[i] = v & 0xFF;
  h[i + 1] = (v >> 8) & 0xFF;
}
void putTag(std::array<uint8_t, 44>& h, int i, const char* tag) {
  for (int k = 0; k < 4; ++k) h[i + k] = static_cast<uint8_t>(tag[k]);
}
}  // namespace

std::array<uint8_t, 44> wavHeader(uint32_t dataBytes, uint32_t sampleRate) {
  constexpr uint16_t kChannels = 1;
  constexpr uint16_t kBitsPerSample = 16;
  const uint16_t block_align = kChannels * kBitsPerSample / 8;
  const uint32_t byte_rate = sampleRate * block_align;

  std::array<uint8_t, 44> h{};
  putTag(h, 0, "RIFF");
  put32(h, 4, 36 + dataBytes);
  putTag(h, 8, "WAVE");
  putTag(h, 12, "fmt ");
  put32(h, 16, 16);              // PCM fmt chunk size
  put16(h, 20, 1);               // audio format = PCM
  put16(h, 22, kChannels);
  put32(h, 24, sampleRate);
  put32(h, 28, byte_rate);
  put16(h, 32, block_align);
  put16(h, 34, kBitsPerSample);
  putTag(h, 36, "data");
  put32(h, 40, dataBytes);
  return h;
}

}  // namespace pokedex
