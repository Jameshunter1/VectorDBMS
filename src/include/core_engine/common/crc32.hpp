#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace core_engine::crc32 {

constexpr std::uint32_t kDefaultSeed = 0xFFFFFFFFu;

// Incrementally update a CRC32 state with the provided bytes.
std::uint32_t Update(std::uint32_t state, std::span<const std::byte> bytes);

// Finalize a CRC32 computation (bitwise invert per IEEE 802.3 standard).
constexpr std::uint32_t Finalize(std::uint32_t state) {
  return ~state;
}

// Convenience helper for single-span CRC32 computation.
inline std::uint32_t Compute(std::span<const std::byte> bytes) {
  return Finalize(Update(kDefaultSeed, bytes));
}

} // namespace core_engine::crc32
