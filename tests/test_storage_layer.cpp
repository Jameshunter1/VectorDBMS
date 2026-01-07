#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <core_engine/storage/page.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/buffer_pool_manager.hpp>
#include <core_engine/storage/log_manager.hpp>

#include <chrono>
#include <filesystem>

using namespace core_engine;

TEST_CASE("Storage Layer: Page Operations", "[storage][page]") {
  Page page;
  
  SECTION("Default page initialization") {
    REQUIRE(page.GetPageId() == kInvalidPageId);
    REQUIRE(page.GetLSN() == 0);
    REQUIRE(page.GetPinCount() == 0);
    REQUIRE_FALSE(page.IsDirty());
  }
  
  SECTION("Page metadata") {
    page.SetPageId(42);
    page.SetLSN(1000);
    page.MarkDirty();
    
    REQUIRE(page.GetPageId() == 42);
    REQUIRE(page.GetLSN() == 1000);
    REQUIRE(page.IsDirty());
  }
  
  SECTION("Checksum operations") {
    page.SetPageId(1);
    page.SetLSN(123);
    
    // Update checksum
    page.UpdateChecksum();
    
    // Verify should pass
    REQUIRE(page.VerifyChecksum());
    
    // Corrupt data
    page.GetData()[0] = 'X';
    
    // Verify should fail
    REQUIRE_FALSE(page.VerifyChecksum());
  }
  
  SECTION("Pin count management") {
    REQUIRE(page.GetPinCount() == 0);
    
    page.IncrementPinCount();
    REQUIRE(page.GetPinCount() == 1);
    
    page.IncrementPinCount();
    REQUIRE(page.GetPinCount() == 2);
    
    page.DecrementPinCount();
    REQUIRE(page.GetPinCount() == 1);
    
    page.DecrementPinCount();
    REQUIRE(page.GetPinCount() == 0);
    
    // Should not go below 0
    page.DecrementPinCount();
    REQUIRE(page.GetPinCount() == 0);
  }
  
  SECTION("Page size constants") {
    REQUIRE(Page::Size() == 4096);
    REQUIRE(Page::DataSize() == 4032);
    REQUIRE(sizeof(Page) == 4096);
  }
}

TEST_CASE("Storage Layer: DiskManager", "[storage][disk]") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("test_disk_manager_" + std::to_string(suffix));
  
  std::filesystem::create_directories(db_dir);
  
  SECTION("Open and close") {
    DiskManager dm(db_dir / "test.db");
    
    REQUIRE(dm.Open().ok());
    REQUIRE(dm.IsOpen());
    
    dm.Close();
    REQUIRE_FALSE(dm.IsOpen());
  }
  
  SECTION("Allocate pages") {
    DiskManager dm(db_dir / "alloc.db");
    REQUIRE(dm.Open().ok());
    
    auto page1 = dm.AllocatePage();
    auto page2 = dm.AllocatePage();
    auto page3 = dm.AllocatePage();
    
    REQUIRE(page1 < page2);
    REQUIRE(page2 < page3);
    REQUIRE(dm.GetNumPages() == 3);
    
    dm.Close();
  }
  
  SECTION("Write and read pages") {
    DiskManager dm(db_dir / "io.db");
    REQUIRE(dm.Open().ok());
    
    // Create page
    Page write_page;
    write_page.SetPageId(42);
    write_page.SetLSN(1000);
    std::memcpy(write_page.GetData(), "Hello, World!", 13);
    write_page.UpdateChecksum();
    
    // Write page
    auto page_id = dm.AllocatePage();
    REQUIRE(dm.WritePage(page_id, &write_page).ok());
    
    // Read page back
    Page read_page;
    REQUIRE(dm.ReadPage(page_id, &read_page).ok());
    
    // Verify
    REQUIRE(read_page.GetPageId() == 42);
    REQUIRE(read_page.GetLSN() == 1000);
    REQUIRE(std::memcmp(read_page.GetData(), "Hello, World!", 13) == 0);
    REQUIRE(read_page.VerifyChecksum());
    
    dm.Close();
  }
  
  SECTION("Statistics") {
    DiskManager dm(db_dir / "stats.db");
    REQUIRE(dm.Open().ok());
    
    Page page;
    page.UpdateChecksum();
    
    // Perform some operations
    auto page1 = dm.AllocatePage();
    dm.WritePage(page1, &page);
    dm.ReadPage(page1, &page);
    
    auto stats = dm.GetStats();
    REQUIRE(stats.total_writes == 1);
    REQUIRE(stats.total_reads == 1);
    REQUIRE(stats.total_allocations == 1);
    
    dm.Close();
  }
  
  std::filesystem::remove_all(db_dir);
}

TEST_CASE("Storage Layer: BufferPoolManager", "[storage][buffer]") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("test_buffer_pool_" + std::to_string(suffix));
  
  std::filesystem::create_directories(db_dir);
  
  DiskManager dm(db_dir / "test.db");
  REQUIRE(dm.Open().ok());
  
  SECTION("Basic operations") {
    BufferPoolManager bpm(10, &dm);
    
    // Allocate new page
    PageId page_id;
    Page* page = bpm.NewPage(&page_id);
    
    REQUIRE(page != nullptr);
    REQUIRE(page->GetPageId() != kInvalidPageId);
    
    // Modify page
    std::memcpy(page->GetData(), "Test data", 9);
    
    // Unpin
    REQUIRE(bpm.UnpinPage(page_id, true));
    
    // Fetch again
    Page* page2 = bpm.FetchPage(page_id);
    REQUIRE(page2 != nullptr);
    REQUIRE(std::memcmp(page2->GetData(), "Test data", 9) == 0);
    
    bpm.UnpinPage(page_id, false);
  }
  
  SECTION("Cache hits and misses") {
    BufferPoolManager bpm(5, &dm);  // Small pool
    
    std::vector<PageId> page_ids;
    
    // Fill pool
    for (int i = 0; i < 5; i++) {
      PageId pid;
      Page* p = bpm.NewPage(&pid);
      REQUIRE(p != nullptr);
      page_ids.push_back(pid);
      bpm.UnpinPage(pid, true);
    }
    
    // Fetch from pool (cache hit)
    Page* hit = bpm.FetchPage(page_ids[0]);
    REQUIRE(hit != nullptr);
    bpm.UnpinPage(page_ids[0], false);
    
    // Allocate more pages (causes eviction)
    for (int i = 0; i < 10; i++) {
      PageId pid;
      Page* p = bpm.NewPage(&pid);
      if (p) {
        bpm.UnpinPage(pid, true);
      }
    }
    
    auto stats = bpm.GetStats();
    REQUIRE(stats.cache_hits > 0);
    REQUIRE(stats.cache_misses >= 0);
  }
  
  SECTION("Flush operations") {
    BufferPoolManager bpm(10, &dm);
    
    PageId page_id;
    Page* page = bpm.NewPage(&page_id);
    REQUIRE(page != nullptr);
    
    std::memcpy(page->GetData(), "Flush test", 10);
    bpm.UnpinPage(page_id, true);
    
    // Flush specific page
    REQUIRE(bpm.FlushPage(page_id));
    
    // Flush all pages
    REQUIRE(bpm.FlushAllPages());
  }
  
  dm.Close();
  std::filesystem::remove_all(db_dir);
}

TEST_CASE("Storage Layer: LogManager", "[storage][wal]") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto wal_file = std::filesystem::temp_directory_path() /
                        ("test_wal_" + std::to_string(suffix) + ".log");
  
  SECTION("Basic log operations") {
    LogManager lm(wal_file.string());
    
    TxnId txn_id = 1;
    
    // Begin transaction
    auto begin_lsn = lm.AppendBeginRecord(txn_id);
    REQUIRE(begin_lsn > 0);
    
    // Update record
    std::byte old_data[] = {std::byte{1}, std::byte{2}, std::byte{3}};
    std::byte new_data[] = {std::byte{4}, std::byte{5}, std::byte{6}};
    
    auto update_lsn = lm.AppendUpdateRecord(
        txn_id, begin_lsn, 1, 0, 3, old_data, new_data);
    REQUIRE(update_lsn > begin_lsn);
    
    // Commit transaction
    auto commit_lsn = lm.AppendCommitRecord(txn_id, update_lsn);
    REQUIRE(commit_lsn > update_lsn);
    
    // Force flush
    REQUIRE(lm.ForceFlush().ok());
  }
  
  SECTION("LSN ordering") {
    LogManager lm(wal_file.string());
    
    auto lsn1 = lm.AppendBeginRecord(1);
    auto lsn2 = lm.AppendBeginRecord(2);
    auto lsn3 = lm.AppendBeginRecord(3);
    
    // LSNs should be monotonically increasing
    REQUIRE(lsn1 < lsn2);
    REQUIRE(lsn2 < lsn3);
  }
  
  std::filesystem::remove(wal_file);
}

TEST_CASE("Storage Layer: Performance Benchmarks", "[storage][benchmark]") {
  Page page;
  
  BENCHMARK("Page checksum computation") {
    return page.ComputeChecksum();
  };
  
  BENCHMARK("Page checksum verification") {
    page.UpdateChecksum();
    return page.VerifyChecksum();
  };
}
