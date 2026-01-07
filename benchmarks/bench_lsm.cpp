#include <benchmark/benchmark.h>

#include <core_engine/engine.hpp>
#include <core_engine/lsm/wal.hpp>
#include <core_engine/lsm/memtable.hpp>
#include <core_engine/lsm/sstable.hpp>

#include <filesystem>
#include <random>
#include <string>

namespace {

// Helper: Generate random key-value pairs
std::pair<std::string, std::string> GenerateKV(std::mt19937& rng, size_t value_size = 100) {
  std::uniform_int_distribution<int> dist(0, 999999);
  std::string key = "key_" + std::to_string(dist(rng));
  std::string value(value_size, 'x');
  return {key, value};
}

// Setup/teardown for benchmarks needing a database directory
const auto g_bench_dir = std::filesystem::temp_directory_path() / "core_engine_bench";

void SetupBenchDir() {
  std::error_code ec;
  std::filesystem::create_directories(g_bench_dir, ec);
}

void CleanupBenchDir() {
  std::error_code ec;
  std::filesystem::remove_all(g_bench_dir, ec);
}

} // namespace

// ============================================================================
// WAL Benchmarks
// ============================================================================

static void BM_WAL_Append(benchmark::State& state) {
  SetupBenchDir();
  core_engine::Wal wal(g_bench_dir / "bench_wal.log");
  
  if (!wal.OpenOrCreate().ok()) {
    state.SkipWithError("Failed to open WAL");
    return;
  }

  std::mt19937 rng(42);
  for (auto _ : state) {
    auto [key, value] = GenerateKV(rng, 100);
    auto status = wal.AppendPut(key, value);
    if (!status.ok()) {
      state.SkipWithError("WAL append failed");
      return;
    }
  }

  state.SetBytesProcessed(state.iterations() * (100 + 10)); // ~100 byte value + key
  CleanupBenchDir();
}
BENCHMARK(BM_WAL_Append);

static void BM_WAL_AppendLarge(benchmark::State& state) {
  SetupBenchDir();
  core_engine::Wal wal(g_bench_dir / "bench_wal_large.log");
  
  if (!wal.OpenOrCreate().ok()) {
    state.SkipWithError("Failed to open WAL");
    return;
  }

  std::mt19937 rng(42);
  const size_t value_size = 1024; // 1 KB values
  
  for (auto _ : state) {
    auto [key, value] = GenerateKV(rng, value_size);
    auto status = wal.AppendPut(key, value);
    if (!status.ok()) {
      state.SkipWithError("WAL append failed");
      return;
    }
  }

  state.SetBytesProcessed(state.iterations() * (value_size + 10));
  CleanupBenchDir();
}
BENCHMARK(BM_WAL_AppendLarge);

// ============================================================================
// MemTable Benchmarks
// ============================================================================

static void BM_MemTable_Insert(benchmark::State& state) {
  core_engine::MemTable memtable;
  std::mt19937 rng(42);
  
  for (auto _ : state) {
    auto [key, value] = GenerateKV(rng);
    memtable.Put(key, value);
  }
  
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MemTable_Insert);

static void BM_MemTable_Get(benchmark::State& state) {
  core_engine::MemTable memtable;
  std::mt19937 rng(42);
  
  // Pre-populate with 10,000 entries
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; ++i) {
    auto [key, value] = GenerateKV(rng);
    memtable.Put(key, value);
    if (i % 10 == 0) keys.push_back(key); // Save some keys for lookup
  }
  
  size_t key_idx = 0;
  for (auto _ : state) {
    auto result = memtable.Get(keys[key_idx % keys.size()]);
    benchmark::DoNotOptimize(result);
    ++key_idx;
  }
  
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MemTable_Get);

// ============================================================================
// SSTable Benchmarks
// ============================================================================

static void BM_SSTable_Write(benchmark::State& state) {
  SetupBenchDir();
  std::mt19937 rng(42);
  
  for (auto _ : state) {
    state.PauseTiming();
    
    // Prepare 1000 sorted entries
    std::map<std::string, std::string> entries;
    for (int i = 0; i < 1000; ++i) {
      entries[std::to_string(i * 100)] = std::string(100, 'x');
    }
    
    const auto sstable_path = g_bench_dir / ("bench_sstable_" + std::to_string(state.iterations()) + ".sst");
    core_engine::SSTableWriter writer(sstable_path);
    
    state.ResumeTiming();
    
    if (!writer.Open().ok()) {
      state.SkipWithError("Failed to open SSTable writer");
      return;
    }
    
    for (const auto& [key, value] : entries) {
      if (!writer.Add(key, value).ok()) {
        state.SkipWithError("Failed to add entry");
        return;
      }
    }
    
    if (!writer.Finish().ok()) {
      state.SkipWithError("Failed to finish SSTable");
      return;
    }
  }
  
  CleanupBenchDir();
}
BENCHMARK(BM_SSTable_Write);

static void BM_SSTable_Read_WithBloom(benchmark::State& state) {
  SetupBenchDir();
  
  // Create an SSTable with 10,000 entries
  const auto sstable_path = g_bench_dir / "bench_sstable_read.sst";
  {
    core_engine::SSTableWriter writer(sstable_path);
    writer.Open();
    for (int i = 0; i < 10000; ++i) {
      writer.Add(std::to_string(i * 100), std::string(100, 'x'));
    }
    writer.Finish();
  }
  
  core_engine::SSTableReader reader(sstable_path);
  if (!reader.Open().ok()) {
    state.SkipWithError("Failed to open SSTable");
    return;
  }
  
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(0, 9999);
  
  for (auto _ : state) {
    int idx = dist(rng);
    std::string key = std::to_string(idx * 100);
    auto result = reader.Get(key);
    benchmark::DoNotOptimize(result);
  }
  
  // Report bloom filter effectiveness
  state.counters["bloom_checks"] = reader.GetBloomFilterChecks();
  state.counters["bloom_hits"] = reader.GetBloomFilterHits();
  state.counters["bloom_fps"] = reader.GetBloomFilterFalsePositives();
  
  CleanupBenchDir();
}
BENCHMARK(BM_SSTable_Read_WithBloom);

static void BM_SSTable_Read_MissWithBloom(benchmark::State& state) {
  SetupBenchDir();
  
  // Create an SSTable with 10,000 entries
  const auto sstable_path = g_bench_dir / "bench_sstable_miss.sst";
  {
    core_engine::SSTableWriter writer(sstable_path);
    writer.Open();
    for (int i = 0; i < 10000; ++i) {
      writer.Add(std::to_string(i * 100), std::string(100, 'x'));
    }
    writer.Finish();
  }
  
  core_engine::SSTableReader reader(sstable_path);
  if (!reader.Open().ok()) {
    state.SkipWithError("Failed to open SSTable");
    return;
  }
  
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(0, 9999);
  
  for (auto _ : state) {
    // Search for keys that DON'T exist (odd numbers when we only stored even)
    int idx = dist(rng) * 2 + 1;
    std::string key = std::to_string(idx * 100);
    auto result = reader.Get(key);
    benchmark::DoNotOptimize(result);
  }
  
  // Bloom filter should reject most misses quickly
  state.counters["bloom_checks"] = reader.GetBloomFilterChecks();
  state.counters["bloom_hits"] = reader.GetBloomFilterHits();
  state.counters["bloom_fps"] = reader.GetBloomFilterFalsePositives();
  
  CleanupBenchDir();
}
BENCHMARK(BM_SSTable_Read_MissWithBloom);

// ============================================================================
// End-to-End Engine Benchmarks
// ============================================================================

static void BM_Engine_Put(benchmark::State& state) {
  SetupBenchDir();
  
  core_engine::Engine engine;
  if (!engine.Open(g_bench_dir / "bench_engine").ok()) {
    state.SkipWithError("Failed to open engine");
    return;
  }
  
  std::mt19937 rng(42);
  
  for (auto _ : state) {
    auto [key, value] = GenerateKV(rng);
    auto status = engine.Put(key, value);
    if (!status.ok()) {
      state.SkipWithError("Put failed");
      return;
    }
  }
  
  state.SetItemsProcessed(state.iterations());
  CleanupBenchDir();
}
BENCHMARK(BM_Engine_Put);

static void BM_Engine_Get(benchmark::State& state) {
  SetupBenchDir();
  
  core_engine::Engine engine;
  if (!engine.Open(g_bench_dir / "bench_engine_get").ok()) {
    state.SkipWithError("Failed to open engine");
    return;
  }
  
  std::mt19937 rng(42);
  
  // Pre-populate with 10,000 entries
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; ++i) {
    auto [key, value] = GenerateKV(rng);
    engine.Put(key, value);
    if (i % 10 == 0) keys.push_back(key);
  }
  
  size_t key_idx = 0;
  for (auto _ : state) {
    auto result = engine.Get(keys[key_idx % keys.size()]);
    benchmark::DoNotOptimize(result);
    ++key_idx;
  }
  
  state.SetItemsProcessed(state.iterations());
  CleanupBenchDir();
}
BENCHMARK(BM_Engine_Get);

static void BM_Engine_Mixed_Workload(benchmark::State& state) {
  SetupBenchDir();
  
  core_engine::Engine engine;
  if (!engine.Open(g_bench_dir / "bench_engine_mixed").ok()) {
    state.SkipWithError("Failed to open engine");
    return;
  }
  
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> op_dist(0, 99);
  
  // Pre-populate
  std::vector<std::string> keys;
  for (int i = 0; i < 1000; ++i) {
    auto [key, value] = GenerateKV(rng);
    engine.Put(key, value);
    keys.push_back(key);
  }
  
  size_t key_idx = 0;
  for (auto _ : state) {
    int op = op_dist(rng);
    if (op < 80) {
      // 80% reads
      auto result = engine.Get(keys[key_idx % keys.size()]);
      benchmark::DoNotOptimize(result);
      ++key_idx;
    } else {
      // 20% writes
      auto [key, value] = GenerateKV(rng);
      engine.Put(key, value);
    }
  }
  
  state.SetItemsProcessed(state.iterations());
  CleanupBenchDir();
}
BENCHMARK(BM_Engine_Mixed_Workload);
