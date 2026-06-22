#pragma once

#include <array>
#include <cstdint>

namespace pokedex {

// Build the 44-byte WAV (RIFF/PCM) header for mono 16-bit audio carrying
// `dataBytes` of sample payload at `sampleRate` Hz. Prepend this to the raw
// PCM to make a file the whisper.cpp server accepts.
std::array<uint8_t, 44> wavHeader(uint32_t dataBytes, uint32_t sampleRate);

}  // namespace pokedex
