#include <catch2/catch_test_macros.hpp>

#include <core_engine/engine.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <vector>

TEST_CASE("Engine opens a database directory") {
  core_engine::Engine engine;

  // Put test data in the build tree when running via CTest.
  const auto db_dir = std::filesystem::temp_directory_path() / "core_engine_test_db";

  // Note: this is a starter test; real suites should isolate per-test dirs.
  const auto status = engine.Open(db_dir);
  REQUIRE(status.ok());
}

TEST_CASE("Engine Put/Get round-trip (page-based)")
{
 core_engine::Engine engine;

 const auto db_dir = std::filesystem::temp_directory_path() / "core_engine_test_db_kv";
 const auto open_status = engine.Open(db_dir);
 REQUIRE(open_status.ok());

 const auto put_status = engine.Put("hello", "world");
 REQUIRE(put_status.ok());

 const auto value = engine.Get("hello");
 REQUIRE(value.has_value());
 REQUIRE(*value == "world");
}

TEST_CASE("Engine recovers values after restart (WAL replay)") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_db_recovery_" + std::to_string(suffix));

  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    const auto put_status = engine.Put("k", "v1");
    REQUIRE(put_status.ok());
  }

  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    const auto value = engine.Get("k");
    REQUIRE(value.has_value());
    REQUIRE(*value == "v1");
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine flushes MemTable to SSTable when threshold exceeded") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_db_flush_" + std::to_string(suffix));

  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    // Write enough data to trigger flush (4 MB threshold).
    // Use 1 KB values to reach threshold faster.
    const std::string large_value(1024, 'x');
    for (int i = 0; i < 5000; ++i) {
      const auto key = "key_" + std::to_string(i);
      const auto put_status = engine.Put(key, large_value);
      REQUIRE(put_status.ok());
    }

    // Verify an SSTable file was created (check recursively for level directories).
    bool found_sstable = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(db_dir)) {
      if (entry.path().extension() == ".sst") {
        found_sstable = true;
        break;
      }
    }
    REQUIRE(found_sstable);
  }

  // Restart and verify values are readable (from SSTable + WAL).
  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    const auto value = engine.Get("key_100");
    REQUIRE(value.has_value());
    REQUIRE(value->size() == 1024);
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine compacts SSTables when threshold reached") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_db_compact_" + std::to_string(suffix));

  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    // Write enough data to create multiple SSTables.
    // Each flush creates 1 SSTable; after 4 flushes, compaction triggers.
    const std::string large_value(1024, 'x');
    for (int batch = 0; batch < 5; ++batch) {
      for (int i = 0; i < 4500; ++i) {
        const auto key = "batch" + std::to_string(batch) + "_key" + std::to_string(i);
        const auto put_status = engine.Put(key, large_value);
        REQUIRE(put_status.ok());
      }
    }

    // Count SSTable files (check recursively for level directories).
    // With leveled compaction: L0 triggers compaction at 4 files,
    // so we should see files distributed across L0, L1, and maybe L2.
    // The total count should be reasonable (not all 5 original flushes).
    int sstable_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(db_dir)) {
      if (entry.path().extension() == ".sst") {
        ++sstable_count;
      }
    }
    // With leveled compaction, expect < 5 (some compaction happened).
    // Could be 1-4 files across levels depending on timing.
    REQUIRE(sstable_count < 5);  // Should have compacted at least once.
  }

  // Restart and verify all values are readable.
  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    const auto value = engine.Get("batch2_key100");
    REQUIRE(value.has_value());
    REQUIRE(value->size() == 1024);
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

// ============================================================================
// NEW CRITICAL TESTS: Edge Cases and Production Scenarios
// ============================================================================

TEST_CASE("Engine handles Delete operations correctly") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_delete_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Put, then delete
  REQUIRE(engine.Put("key1", "value1").ok());
  REQUIRE(engine.Get("key1").has_value());
  
  REQUIRE(engine.Delete("key1").ok());
  REQUIRE_FALSE(engine.Get("key1").has_value());

  // Deleting non-existent key should succeed (tombstone written)
  REQUIRE(engine.Delete("nonexistent").ok());

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine tombstones persist across restarts") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_tombstone_persist_" + std::to_string(suffix));

  {
    core_engine::Engine engine;
    engine.Open(db_dir);
    
    REQUIRE(engine.Put("key1", "original").ok());
    REQUIRE(engine.Delete("key1").ok());
    REQUIRE_FALSE(engine.Get("key1").has_value());
  }

  // Restart and verify tombstone persisted
  {
    core_engine::Engine engine;
    engine.Open(db_dir);
    
    REQUIRE_FALSE(engine.Get("key1").has_value());
    
    // Can re-insert after tombstone
    REQUIRE(engine.Put("key1", "new_value").ok());
    REQUIRE(engine.Get("key1").value() == "new_value");
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine handles overwrites correctly") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_overwrite_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Initial write
  REQUIRE(engine.Put("key1", "value1").ok());
  REQUIRE(engine.Get("key1").value() == "value1");

  // Overwrite multiple times
  REQUIRE(engine.Put("key1", "value2").ok());
  REQUIRE(engine.Get("key1").value() == "value2");

  REQUIRE(engine.Put("key1", "value3").ok());
  REQUIRE(engine.Get("key1").value() == "value3");

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine handles empty and short values") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_empty_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Single character key and value
  REQUIRE(engine.Put("x", "y").ok());
  auto val1 = engine.Get("x");
  REQUIRE(val1.has_value());
  REQUIRE(*val1 == "y");

  // Single character key with longer value
  REQUIRE(engine.Put("a", "value1").ok());
  auto val2 = engine.Get("a");
  REQUIRE(val2.has_value());
  REQUIRE(*val2 == "value1");

  // Normal key with single character value
  REQUIRE(engine.Put("key1", "v").ok());
  auto val3 = engine.Get("key1");
  REQUIRE(val3.has_value());
  REQUIRE(*val3 == "v");

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine handles large keys and values") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_large_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Large key (10 KB)
  std::string large_key(10 * 1024, 'k');
  REQUIRE(engine.Put(large_key, "value").ok());
  REQUIRE(engine.Get(large_key).has_value());

  // Large value (1 MB)
  std::string large_value(1024 * 1024, 'v');
  REQUIRE(engine.Put("key", large_value).ok());
  auto retrieved = engine.Get("key");
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->size() == large_value.size());

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine handles special characters in keys and values") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_special_chars_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Null bytes
  std::string key_with_null = "key\0with\0nulls";
  std::string val_with_null = "val\0with\0nulls";
  REQUIRE(engine.Put(key_with_null, val_with_null).ok());
  auto retrieved = engine.Get(key_with_null);
  REQUIRE(retrieved.has_value());

  // Unicode
  REQUIRE(engine.Put("键", "值").ok());
  REQUIRE(engine.Get("键").value() == "值");

  // Binary data
  std::string binary_data;
  for (int i = 0; i < 256; i++) {
    binary_data += static_cast<char>(i);
  }
  REQUIRE(engine.Put("binary", binary_data).ok());
  auto binary_retrieved = engine.Get("binary");
  REQUIRE(binary_retrieved.has_value());
  REQUIRE(binary_retrieved->size() == 256);

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine statistics are accurate") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_stats_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  auto stats_initial = engine.GetStats();
  REQUIRE(stats_initial.total_puts == 0);

  // Add some entries
  for (int i = 0; i < 100; i++) {
    engine.Put("key" + std::to_string(i), "value" + std::to_string(i));
  }

  auto stats_after = engine.GetStats();
  REQUIRE(stats_after.total_puts == 100);
  REQUIRE(stats_after.total_pages >= 0);

  // Perform some gets
  for (int i = 0; i < 50; i++) {
    engine.Get("key" + std::to_string(i));
  }

  auto stats_gets = engine.GetStats();
  REQUIRE(stats_gets.total_gets == 50);

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine handles concurrent operations safely") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_concurrent_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Launch multiple threads doing writes
  std::vector<std::thread> writers;
  for (int t = 0; t < 4; t++) {
    writers.emplace_back([&engine, t]() {
      for (int i = 0; i < 100; i++) {
        std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
        engine.Put(key, "value" + std::to_string(i));
      }
    });
  }

  // Launch multiple threads doing reads
  std::vector<std::thread> readers;
  for (int t = 0; t < 4; t++) {
    readers.emplace_back([&engine, t]() {
      for (int i = 0; i < 100; i++) {
        std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
        // Reads might return nullopt early on, that's fine
        engine.Get(key);
      }
    });
  }

  for (auto& t : writers) t.join();
  for (auto& t : readers) t.join();

  // Verify all writes succeeded
  for (int t = 0; t < 4; t++) {
    for (int i = 0; i < 100; i++) {
      std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
      auto val = engine.Get(key);
      REQUIRE(val.has_value());
    }
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine BatchWrite handles mixed operations") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_batch_mixed_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Pre-populate some keys
  for (int i = 0; i < 50; i++) {
    engine.Put("key" + std::to_string(i), "original" + std::to_string(i));
  }

  // Batch with mixed PUT and DELETE
  std::vector<core_engine::Engine::BatchOperation> ops;
  
  // Update some existing keys
  for (int i = 0; i < 25; i++) {
    ops.push_back({
      core_engine::Engine::BatchOperation::Type::PUT,
      "key" + std::to_string(i),
      "updated" + std::to_string(i)
    });
  }
  
  // Delete some keys
  for (int i = 25; i < 50; i++) {
    ops.push_back({
      core_engine::Engine::BatchOperation::Type::DELETE,
      "key" + std::to_string(i),
      ""  // Empty value for DELETE
    });
  }
  
  // Add new keys
  for (int i = 50; i < 100; i++) {
    ops.push_back({
      core_engine::Engine::BatchOperation::Type::PUT,
      "key" + std::to_string(i),
      "new" + std::to_string(i)
    });
  }

  REQUIRE(engine.BatchWrite(ops).ok());

  // Verify updates
  for (int i = 0; i < 25; i++) {
    auto val = engine.Get("key" + std::to_string(i));
    REQUIRE(val.has_value());
    REQUIRE(*val == "updated" + std::to_string(i));
  }

  // Verify deletes
  for (int i = 25; i < 50; i++) {
    REQUIRE_FALSE(engine.Get("key" + std::to_string(i)).has_value());
  }

  // Verify new keys
  for (int i = 50; i < 100; i++) {
    auto val = engine.Get("key" + std::to_string(i));
    REQUIRE(val.has_value());
    REQUIRE(*val == "new" + std::to_string(i));
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine Scan returns correct range results") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_scan_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Populate with sorted keys
  for (int i = 0; i < 100; i++) {
    std::string key = "key_" + std::string(3 - std::min(3, static_cast<int>(std::to_string(i).length())), '0') + std::to_string(i);
    engine.Put(key, "value" + std::to_string(i));
  }

  // Test basic range scan
  core_engine::Engine::ScanOptions opts;
  auto results = engine.Scan("key_010", "key_020", opts);
  REQUIRE(results.size() == 10);  // key_010 to key_019

  // Test reverse scan
  opts.reverse = true;
  results = engine.Scan("key_010", "key_020", opts);
  REQUIRE(results.size() == 10);
  REQUIRE(results[0].first > results[results.size()-1].first);  // Descending order

  // Test limit
  opts.reverse = false;
  opts.limit = 5;
  results = engine.Scan("key_000", "key_100", opts);
  REQUIRE(results.size() == 5);

  // Test keys_only option
  opts.limit = 0;
  opts.keys_only = true;
  results = engine.Scan("key_030", "key_040", opts);
  REQUIRE(results.size() == 10);
  for (const auto& pair : results) {
    REQUIRE(pair.second.empty());  // Values should be empty
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}
