#pragma once

#include <cstdint>
#include <filesystem>

#include <core_engine/common/status.hpp>
#include <core_engine/storage/page.hpp>

namespace core_engine {

// PageFile is a minimal disk abstraction.
//
// This is intentionally not an mmap wrapper (yet). Databases often need:
// - explicit fsync control
// - predictable write ordering (WAL before data)
// - preallocation / direct I/O
// - platform-specific optimizations
//
// This class is a placeholder seam where your real storage engine will grow.
class PageFile {
public:
  explicit PageFile(std::filesystem::path path);

  Status OpenOrCreate();
  Status Read(PageId id, Page* out_page);
  Status Write(PageId id, const Page& page);

private:
  std::filesystem::path path_;
};

} // namespace core_engine
