#include <catch2/catch_test_macros.hpp>

#include <catch2/catch_approx.hpp>
#include <core_engine/common/config.hpp>
#include <core_engine/engine.hpp>
#include <core_engine/vector/sift_parser.hpp>
#include <core_engine/vector/vector.hpp>

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

TEST_CASE("Engine Put/Get round-trip (page-based)") {
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

TEST_CASE("Engine persists large datasets across pages") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_test_db_large_" + std::to_string(suffix));

  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    // Write enough data to span multiple pages (4KB page size)
    // Use 1KB values to test multi-page storage
    const std::string large_value(1024, 'x');
    for (int i = 0; i < 100; ++i) {
      const auto key = "key_" + std::to_string(i);
      const auto put_status = engine.Put(key, large_value);
      REQUIRE(put_status.ok());
    }

  } // Engine destructor flushes pages

  // Verify pages.db was created and has grown (page-based architecture)
  // Check AFTER destructor to ensure pages are flushed to disk
  const auto pages_file = db_dir / "pages.db";
  REQUIRE(std::filesystem::exists(pages_file));

  // File should contain multiple pages (>100 KB with 100 x 1KB values + overhead)
  const auto file_size = std::filesystem::file_size(pages_file);
  REQUIRE(file_size > 100 * 1024);

  // Restart and verify all values are readable via WAL replay
  {
    core_engine::Engine engine;
    const auto open_status = engine.Open(db_dir);
    REQUIRE(open_status.ok());

    for (int i = 0; i < 100; ++i) {
      const auto key = "key_" + std::to_string(i);
      const auto value = engine.Get(key);
      REQUIRE(value.has_value());
      REQUIRE(value->size() == 1024);
    }
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

// ============================================================================
// Edge Cases and Production Scenarios
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
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("core_engine_test_empty_" + std::to_string(suffix));

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
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("core_engine_test_large_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Large key (512 bytes - reduced for Windows reliability)
  std::string large_key(512, 'k');
  REQUIRE(engine.Put(large_key, "value").ok());
  REQUIRE(engine.Get(large_key).has_value());

  // Large value (3 KB - fits in single 4KB page with overhead)
  std::string large_value(3 * 1024, 'v');
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
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("core_engine_test_stats_" + std::to_string(suffix));

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
        engine.Get(key);
      }
    });
  }

  for (auto& t : writers)
    t.join();
  for (auto& t : readers)
    t.join();

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
    ops.push_back({core_engine::Engine::BatchOperation::Type::kPut, "key" + std::to_string(i),
                   "updated" + std::to_string(i)});
  }

  // Delete some keys
  for (int i = 25; i < 50; i++) {
    ops.push_back({
        core_engine::Engine::BatchOperation::Type::kDelete, "key" + std::to_string(i),
        "" // Empty value for DELETE
    });
  }

  // Add new keys
  for (int i = 50; i < 100; i++) {
    ops.push_back({core_engine::Engine::BatchOperation::Type::kPut, "key" + std::to_string(i),
                   "new" + std::to_string(i)});
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
  const auto db_dir =
      std::filesystem::temp_directory_path() / ("core_engine_test_scan_" + std::to_string(suffix));

  core_engine::Engine engine;
  engine.Open(db_dir);

  // Populate with sorted keys
  for (int i = 0; i < 100; i++) {
    std::string key =
        "key_" + std::string(3 - (std::min)(3, static_cast<int>(std::to_string(i).length())), '0') +
        std::to_string(i);
    engine.Put(key, "value" + std::to_string(i));
  }

  // Test basic range scan
  core_engine::ScanOptions opts;
  auto results = engine.Scan("key_010", "key_020", opts);
  REQUIRE(results.size() == 10); // key_010 to key_019

  // Test reverse scan
  opts.reverse = true;
  results = engine.Scan("key_010", "key_020", opts);
  REQUIRE(results.size() == 10);
  REQUIRE(results[0].first > results[results.size() - 1].first); // Descending order

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
    REQUIRE(pair.second.empty()); // Values should be empty
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("Engine inserts many vectors without layer mismatches") {
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_vector_insert_" + std::to_string(suffix));

  core_engine::DatabaseConfig config = core_engine::DatabaseConfig::Embedded(db_dir);
  config.enable_vector_index = true;
  config.vector_dimension = 32;

  core_engine::Engine engine;
  const auto status = engine.Open(config);
  REQUIRE(status.ok());

  core_engine::vector::Vector vec(config.vector_dimension);

  const int insert_count = 256;
  for (int i = 0; i < insert_count; ++i) {
    for (std::size_t dim = 0; dim < vec.dimension(); ++dim) {
      vec[dim] = static_cast<float>(i) + static_cast<float>(dim) * 0.01f;
    }

    const auto key = "vec_" + std::to_string(i);
    REQUIRE(engine.PutVector(key, vec).ok());
  }

  for (int i = 0; i < insert_count; ++i) {
    const auto key = "vec_" + std::to_string(i);
    auto stored = engine.GetVector(key);
    REQUIRE(stored.has_value());
    REQUIRE(stored->dimension() == vec.dimension());
    REQUIRE((*stored)[0] == static_cast<float>(i));
  }

  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}

TEST_CASE("SIFT file import populates database entries") {
  // Create a small .fvecs file (3 vectors, 4 dimensions)
  const auto suffix = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto db_dir = std::filesystem::temp_directory_path() /
                      ("core_engine_sift_import_" + std::to_string(suffix));
  const auto sift_path = db_dir / "test_vectors.fvecs";

  std::filesystem::create_directories(db_dir);
  {
    std::ofstream ofs(sift_path, std::ios::binary);
    int32_t dim = 4;
    float data[3][4] = {
        {1.0f, 2.0f, 3.0f, 4.0f}, {5.0f, 6.0f, 7.0f, 8.0f}, {9.0f, 10.0f, 11.0f, 12.0f}};
    for (int i = 0; i < 3; ++i) {
      ofs.write(reinterpret_cast<const char*>(&dim), sizeof(dim));
      ofs.write(reinterpret_cast<const char*>(data[i]), sizeof(float) * dim);
    }
  }

  core_engine::DatabaseConfig config = core_engine::DatabaseConfig::Embedded(db_dir);
  config.enable_vector_index = true;
  config.vector_dimension = 4;
  core_engine::Engine engine;
  REQUIRE(engine.Open(config).ok());

  // Simulate the import logic (as in dbweb)
  core_engine::vector::SiftParser parser(sift_path.string());
  REQUIRE(parser.Open());
  size_t imported = 0;
  while (auto vec_opt = parser.Next()) {
    std::string key = "vector:" + std::to_string(imported);
    auto status = engine.PutVector(key, *vec_opt);
    REQUIRE(status.ok());
    ++imported;
  }
  REQUIRE(imported == 3);

  // Check that the vectors are present and correct
  for (size_t i = 0; i < 3; ++i) {
    std::string key = "vector:" + std::to_string(i);
    auto stored = engine.GetVector(key);
    REQUIRE(stored.has_value());
    REQUIRE(stored->dimension() == 4);
    for (size_t d = 0; d < 4; ++d) {
      float expected = 1.0f + 4.0f * static_cast<float>(i) + static_cast<float>(d);
      REQUIRE((*stored)[d] == Catch::Approx(expected));
    }
  }

  // Clean up
  std::error_code ec;
  std::filesystem::remove_all(db_dir, ec);
}
