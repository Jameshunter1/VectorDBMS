#include <core_engine/common/logger.hpp>
#include <core_engine/common/status.hpp>
#include <core_engine/storage/aligned_buffer.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/page.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using core_engine::AlignedBuffer;
using core_engine::DiskManager;
using core_engine::Log;
using core_engine::LogLevel;
using core_engine::Page;
using core_engine::PageId;
using core_engine::Status;

namespace {

struct DemoConfig {
  fs::path db_directory;
  std::size_t page_round_trips = 4;
  std::size_t contiguous_pages = 4;
  bool reset_file = true;
};

Status WriteSamplePage(DiskManager& manager, PageId page_id, std::size_t sequence) {
  Page page;
  page.Reset(page_id);
  const std::string payload =
      "disk_demo page=" + std::to_string(page_id) + " sequence=" + std::to_string(sequence);
  std::memset(page.GetData(), 0, Page::DataSize());
  std::snprintf(page.GetData(), Page::DataSize(), "%s", payload.c_str());
  page.UpdateChecksum();
  return manager.WritePage(page_id, &page);
}

Status RunSinglePageRoundTrips(DiskManager& manager, std::size_t page_count) {
  if (page_count == 0) {
    return Status::InvalidArgument("page_round_trips must be greater than zero");
  }

  std::vector<PageId> allocated;
  allocated.reserve(page_count);

  for (std::size_t i = 0; i < page_count; ++i) {
    const PageId page_id = manager.AllocatePage();
    if (page_id == core_engine::kInvalidPageId) {
      return Status::Internal("DiskManager returned invalid page id");
    }

    auto write_status = WriteSamplePage(manager, page_id, i);
    if (!write_status.ok()) {
      return write_status;
    }
    allocated.push_back(page_id);
  }

  for (const PageId page_id : allocated) {
    Page page;
    auto read_status = manager.ReadPage(page_id, &page);
    if (!read_status.ok()) {
      return read_status;
    }
    if (!page.VerifyChecksum()) {
      return Status::Corruption("Checksum mismatch for page " + std::to_string(page_id));
    }
  }

  return Status::Ok();
}

Status RunContiguousBatch(DiskManager& manager, std::size_t pages_per_batch) {
  if (pages_per_batch == 0) {
    return Status::InvalidArgument("contiguous_pages must be greater than zero");
  }

  PageId first_page = core_engine::kInvalidPageId;
  for (std::size_t i = 0; i < pages_per_batch; ++i) {
    const PageId page_id = manager.AllocatePage();
    if (page_id == core_engine::kInvalidPageId) {
      return Status::Internal("DiskManager returned invalid page id");
    }
    if (i == 0) {
      first_page = page_id;
    } else if (page_id != first_page + static_cast<PageId>(i)) {
      return Status::Internal("Page allocation was not contiguous; cannot run contiguous demo");
    }
  }

  AlignedBuffer write_buffer(Page::Size() * pages_per_batch);
  auto* base_ptr = static_cast<std::uint8_t*>(write_buffer.data());
  for (std::size_t i = 0; i < pages_per_batch; ++i) {
    auto* page_ptr = base_ptr + (i * Page::Size());
    std::memset(page_ptr, static_cast<int>(i), Page::Size());
    const std::string label = "contiguous block page=" + std::to_string(first_page + i);
    std::snprintf(reinterpret_cast<char*>(page_ptr), Page::Size(), "%s", label.c_str());
  }

  auto write_status = manager.WriteContiguous(first_page, write_buffer.data(), pages_per_batch);
  if (!write_status.ok()) {
    return write_status;
  }

  AlignedBuffer read_buffer(write_buffer.size());
  auto read_status = manager.ReadContiguous(first_page, read_buffer.data(), pages_per_batch);
  if (!read_status.ok()) {
    return read_status;
  }

  if (std::memcmp(write_buffer.data(), read_buffer.data(), write_buffer.size()) != 0) {
    return Status::Corruption("Contiguous read/write comparison failed");
  }

  return Status::Ok();
}

DemoConfig ParseArgs(int argc, char** argv) {
  DemoConfig config;
  config.db_directory = (argc >= 2) ? fs::path(argv[1]) : fs::path("./_disk_demo");
  if (argc >= 3) {
    config.page_round_trips = static_cast<std::size_t>(std::stoull(argv[2]));
  }
  if (argc >= 4) {
    config.contiguous_pages = static_cast<std::size_t>(std::stoull(argv[3]));
  }
  return config;
}

void PrintStats(const DiskManager::Stats& stats) {
  std::cout << "\nDiskManager statistics:\n";
  std::cout << "  Total reads      : " << stats.total_reads << "\n";
  std::cout << "  Total writes     : " << stats.total_writes << "\n";
  std::cout << "  Allocations      : " << stats.total_allocations << "\n";
  std::cout << "  Checksum failures: " << stats.checksum_failures << "\n";
}

} // namespace

int main(int argc, char** argv) {
  const DemoConfig config = ParseArgs(argc, argv);
  const fs::path db_file = config.db_directory / "disk_demo.db";

  try {
    fs::create_directories(config.db_directory);
    if (config.reset_file && fs::exists(db_file)) {
      fs::remove(db_file);
    }
  } catch (const std::exception& ex) {
    std::cerr << "Failed to prepare demo directory: " << ex.what() << "\n";
    return 1;
  }

  std::cout << "Running disk demo against " << db_file << "\n";
  std::cout << "  Pages (single) : " << config.page_round_trips << "\n";
  std::cout << "  Pages (chunked): " << config.contiguous_pages << "\n";

  DiskManager manager(db_file);
  auto status = manager.Open();
  if (!status.ok()) {
    std::cerr << "Failed to open disk file: " << status.ToString() << "\n";
    return 1;
  }

  const auto start_single = std::chrono::steady_clock::now();
  status = RunSinglePageRoundTrips(manager, config.page_round_trips);
  const auto end_single = std::chrono::steady_clock::now();
  if (!status.ok()) {
    std::cerr << "Single page demo failed: " << status.ToString() << "\n";
    return 1;
  }
  const auto single_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_single - start_single).count();
  std::cout << "✓ Single page round-trips complete in " << single_ms << " ms\n";

  const auto start_contig = std::chrono::steady_clock::now();
  status = RunContiguousBatch(manager, config.contiguous_pages);
  const auto end_contig = std::chrono::steady_clock::now();
  if (!status.ok()) {
    std::cerr << "Contiguous demo failed: " << status.ToString() << "\n";
    return 1;
  }
  const auto contig_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_contig - start_contig).count();
  std::cout << "✓ Contiguous read/write demo complete in " << contig_ms << " ms\n";

  auto sync_status = manager.Sync();
  if (!sync_status.ok()) {
    std::cerr << "Warning: Sync failed - " << sync_status.ToString() << "\n";
  }
  PrintStats(manager.GetStats());

  Log(LogLevel::kInfo, "disk_demo finished successfully");
  return 0;
}
