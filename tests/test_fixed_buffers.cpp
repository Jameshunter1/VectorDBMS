#include <catch2/catch_test_macros.hpp>

#include <core_engine/storage/buffer_pool_manager.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/page.hpp>

#include <chrono>
#include <filesystem>
#include <string>

using namespace core_engine;

TEST_CASE("Fixed Buffers: Registration and Unregistration", "[storage][fixed_buffers]") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("test_fixed_buffers_" + std::to_string(suffix));

  std::filesystem::create_directories(db_dir);

  SECTION("Register fixed buffers with DiskManager") {
    DiskManager::Options options;
    options.enable_io_uring = true;
    options.register_fixed_buffers = true;

    DiskManager dm(db_dir / "test.db", options);
    REQUIRE(dm.Open().ok());

    // Create a small buffer pool
    const std::size_t pool_size = 16;
    Page* pages = new Page[pool_size];

    // Register buffers
    std::span<Page> buffer_span(pages, pool_size);
    auto status = dm.RegisterFixedBuffers(buffer_span);

#if defined(CORE_ENGINE_HAS_IO_URING)
    // On platforms with io_uring, registration should succeed
    REQUIRE(status.ok());
    REQUIRE(dm.HasFixedBuffers());
#else
    // On platforms without io_uring, it should return Unimplemented
    REQUIRE(status.code() == StatusCode::kUnimplemented);
    REQUIRE_FALSE(dm.HasFixedBuffers());
#endif

    // Unregister buffers
    dm.UnregisterFixedBuffers();
    REQUIRE_FALSE(dm.HasFixedBuffers());

    delete[] pages;
    dm.Close();
  }

  SECTION("Double registration fails") {
    DiskManager::Options options;
    options.enable_io_uring = true;
    options.register_fixed_buffers = true;

    DiskManager dm(db_dir / "test2.db", options);
    REQUIRE(dm.Open().ok());

    const std::size_t pool_size = 16;
    Page* pages = new Page[pool_size];
    std::span<Page> buffer_span(pages, pool_size);

    auto status1 = dm.RegisterFixedBuffers(buffer_span);
#if defined(CORE_ENGINE_HAS_IO_URING)
    REQUIRE(status1.ok());

    // Try to register again - should fail
    auto status2 = dm.RegisterFixedBuffers(buffer_span);
    REQUIRE_FALSE(status2.ok());
    REQUIRE(status2.code() == StatusCode::kAlreadyExists);
#endif

    dm.UnregisterFixedBuffers();
    delete[] pages;
    dm.Close();
  }

  SECTION("Unregister without registration is safe") {
    DiskManager dm(db_dir / "test3.db");
    REQUIRE(dm.Open().ok());

    // Should not crash or error
    dm.UnregisterFixedBuffers();
    REQUIRE_FALSE(dm.HasFixedBuffers());

    dm.Close();
  }

  std::filesystem::remove_all(db_dir);
}

TEST_CASE("Fixed Buffers: BufferPoolManager Integration", "[storage][fixed_buffers]") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("test_bpm_fixed_buffers_" + std::to_string(suffix));

  std::filesystem::create_directories(db_dir);

  SECTION("Get page span from BufferPoolManager") {
    DiskManager dm(db_dir / "test.db");
    REQUIRE(dm.Open().ok());

    const std::size_t pool_size = 32;
    BufferPoolManager bpm(pool_size, &dm);

    // Get span of all pages
    auto page_span = bpm.GetPageSpan();
    REQUIRE(page_span.size() == pool_size);

    // All pages should be contiguous in memory
    for (std::size_t i = 1; i < page_span.size(); ++i) {
      const std::ptrdiff_t diff = reinterpret_cast<const char*>(&page_span[i]) -
                                  reinterpret_cast<const char*>(&page_span[i - 1]);
      REQUIRE(diff == static_cast<std::ptrdiff_t>(sizeof(Page)));
    }

    dm.Close();
  }

  SECTION("Register BufferPoolManager pages with DiskManager") {
    DiskManager::Options options;
    options.enable_io_uring = true;
    options.register_fixed_buffers = true;

    DiskManager dm(db_dir / "test2.db", options);
    REQUIRE(dm.Open().ok());

    const std::size_t pool_size = 32;
    BufferPoolManager bpm(pool_size, &dm);

    // Register buffer pool with DiskManager
    auto status = dm.RegisterFixedBuffers(bpm.GetPageSpan());

#if defined(CORE_ENGINE_HAS_IO_URING)
    REQUIRE(status.ok());
    REQUIRE(dm.HasFixedBuffers());
#else
    REQUIRE(status.code() == StatusCode::kUnimplemented);
#endif

    dm.UnregisterFixedBuffers();
    dm.Close();
  }

  std::filesystem::remove_all(db_dir);
}

TEST_CASE("Fixed Buffers: I/O Operations", "[storage][fixed_buffers]") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("test_fixed_io_" + std::to_string(suffix));

  std::filesystem::create_directories(db_dir);

  SECTION("Read and write with fixed buffers") {
    DiskManager::Options options;
    options.enable_io_uring = true;
    options.register_fixed_buffers = true;

    DiskManager dm(db_dir / "test.db", options);
    REQUIRE(dm.Open().ok());

    const std::size_t pool_size = 32;
    BufferPoolManager bpm(pool_size, &dm);

    // Register buffers
    auto reg_status = dm.RegisterFixedBuffers(bpm.GetPageSpan());

#if defined(CORE_ENGINE_HAS_IO_URING)
    REQUIRE(reg_status.ok());

    // Allocate a new page
    PageId page_id;
    Page* page = bpm.NewPage(&page_id);
    REQUIRE(page != nullptr);
    REQUIRE(page_id != kInvalidPageId);

    // Write some data
    const char* test_data = "Hello from fixed buffers!";
    std::memcpy(page->GetData(), test_data, std::strlen(test_data) + 1);
    page->UpdateChecksum();

    // Unpin and flush
    REQUIRE(bpm.UnpinPage(page_id, true));
    REQUIRE(bpm.FlushPage(page_id));

    // Fetch the page again (forces disk read)
    bpm.DeletePage(page_id);
    Page* fetched_page = bpm.FetchPage(page_id);
    REQUIRE(fetched_page != nullptr);

    // Verify data
    REQUIRE(std::string(fetched_page->GetData()) == "Hello from fixed buffers!");

    bpm.UnpinPage(page_id, false);
#endif

    dm.UnregisterFixedBuffers();
    dm.Close();
  }

  SECTION("Batch operations with fixed buffers") {
    DiskManager::Options options;
    options.enable_io_uring = true;
    options.register_fixed_buffers = true;

    DiskManager dm(db_dir / "test2.db", options);
    REQUIRE(dm.Open().ok());

    const std::size_t pool_size = 64;
    BufferPoolManager bpm(pool_size, &dm);

    auto reg_status = dm.RegisterFixedBuffers(bpm.GetPageSpan());

#if defined(CORE_ENGINE_HAS_IO_URING)
    REQUIRE(reg_status.ok());

    // Create and write multiple pages
    constexpr int num_pages = 10;
    std::vector<PageId> page_ids;

    for (int i = 0; i < num_pages; ++i) {
      PageId page_id;
      Page* page = bpm.NewPage(&page_id);
      REQUIRE(page != nullptr);

      std::string data = "Page " + std::to_string(i);
      std::memcpy(page->GetData(), data.c_str(), data.size() + 1);
      page->UpdateChecksum();

      page_ids.push_back(page_id);
      REQUIRE(bpm.UnpinPage(page_id, true));
    }

    // Flush all pages
    REQUIRE(bpm.FlushAllPages());

    // Read them back
    for (int i = 0; i < num_pages; ++i) {
      bpm.DeletePage(page_ids[i]);
      Page* page = bpm.FetchPage(page_ids[i]);
      REQUIRE(page != nullptr);

      std::string expected = "Page " + std::to_string(i);
      REQUIRE(std::string(page->GetData()) == expected);

      bpm.UnpinPage(page_ids[i], false);
    }
#endif

    dm.UnregisterFixedBuffers();
    dm.Close();
  }

  std::filesystem::remove_all(db_dir);
}

TEST_CASE("Fixed Buffers: Fallback to Dynamic Buffers", "[storage][fixed_buffers]") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("test_fallback_" + std::to_string(suffix));

  std::filesystem::create_directories(db_dir);

  SECTION("I/O without fixed buffers uses dynamic buffers") {
    DiskManager::Options options;
    options.enable_io_uring = true;
    options.register_fixed_buffers = false; // Explicitly disable

    DiskManager dm(db_dir / "test.db", options);
    REQUIRE(dm.Open().ok());

    const std::size_t pool_size = 32;
    BufferPoolManager bpm(pool_size, &dm);

    // Do NOT register fixed buffers
    REQUIRE_FALSE(dm.HasFixedBuffers());

    // Allocate and write a page (should use dynamic buffers)
    PageId page_id;
    Page* page = bpm.NewPage(&page_id);
    REQUIRE(page != nullptr);

    const char* test_data = "Dynamic buffer test";
    std::memcpy(page->GetData(), test_data, std::strlen(test_data) + 1);
    page->UpdateChecksum();

    REQUIRE(bpm.UnpinPage(page_id, true));
    REQUIRE(bpm.FlushPage(page_id));

    // Read it back
    bpm.DeletePage(page_id);
    Page* fetched = bpm.FetchPage(page_id);
    REQUIRE(fetched != nullptr);
    REQUIRE(std::string(fetched->GetData()) == "Dynamic buffer test");

    bpm.UnpinPage(page_id, false);
    dm.Close();
  }

  std::filesystem::remove_all(db_dir);
}
