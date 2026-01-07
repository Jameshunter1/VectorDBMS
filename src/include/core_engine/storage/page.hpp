#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace core_engine {

// A Page is the fundamental unit of I/O.
//
// Why pages?
// - Databases typically read/write fixed-size blocks for predictable caching.
// - Higher-level structures (B-Tree nodes, WAL records, etc.) live "inside" pages.
//
// This starter template uses a fixed page size (4 KiB) to keep the example small.
// In a real engine you may:
// - make this configurable via EngineConfig
// - support multiple page sizes
// - enforce alignment

constexpr std::size_t kDefaultPageSize = 4096;

using PageId = std::uint64_t;

class Page {
 public:
  Page() = default;

  std::byte* data() { return data_.data(); }
  const std::byte* data() const { return data_.data(); }

  static constexpr std::size_t size() { return kDefaultPageSize; }

 private:
  std::array<std::byte, kDefaultPageSize> data_{};
};

}  // namespace core_engine
