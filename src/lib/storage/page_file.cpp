#include <core_engine/storage/page_file.hpp>

// core_engine/storage/page_file.cpp
//
// Purpose:
// - Minimal file-backed page I/O.
// - Exists mainly as a seam: as the engine grows this becomes a real storage
//   backend (preallocation, direct I/O, checksums, fsync policy, etc.).

#include <fstream>
#include <utility>

namespace core_engine {

PageFile::PageFile(std::filesystem::path path) : path_(std::move(path)) {}

Status PageFile::OpenOrCreate() {
  // Create the file if it doesn't exist.
  // We avoid keeping a long-lived file descriptor in this template.
  // Real engines usually keep a handle open and manage concurrency.
  std::ofstream create(path_, std::ios::binary | std::ios::app);
  if (!create) {
    return Status::IoError("Failed to create/open page file");
  }
  return Status::Ok();
}

Status PageFile::Read(PageId id, Page* out_page) {
  if (out_page == nullptr) {
    return Status::InvalidArgument("out_page is null");
  }

  std::ifstream in(path_, std::ios::binary);
  if (!in) {
    return Status::IoError("Failed to open page file for reading");
  }

  const std::uint64_t offset = id * static_cast<std::uint64_t>(Page::size());
  in.seekg(static_cast<std::streamoff>(offset));
  if (!in) {
    return Status::NotFound("Page offset is beyond end of file");
  }

  in.read(reinterpret_cast<char*>(out_page->data()), static_cast<std::streamsize>(Page::size()));
  if (in.gcount() != static_cast<std::streamsize>(Page::size())) {
    return Status::NotFound("Page is not fully present");
  }

  return Status::Ok();
}

Status PageFile::Write(PageId id, const Page& page) {
  std::fstream io(path_, std::ios::binary | std::ios::in | std::ios::out);
  if (!io) {
    // Attempt to create and reopen.
    auto status = OpenOrCreate();
    if (!status.ok()) {
      return status;
    }
    io.clear();
    io.open(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!io) {
      return Status::IoError("Failed to open page file for writing");
    }
  }

  const std::uint64_t offset = id * static_cast<std::uint64_t>(Page::size());
  io.seekp(static_cast<std::streamoff>(offset));
  if (!io) {
    return Status::IoError("Failed to seek for write");
  }

  io.write(reinterpret_cast<const char*>(page.data()), static_cast<std::streamsize>(Page::size()));
  if (!io) {
    return Status::IoError("Failed to write page");
  }

  return Status::Ok();
}

}  // namespace core_engine
