#include <benchmark/benchmark.h>

#include <core_engine/storage/buffer_pool_manager.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/page.hpp>

#include <filesystem>
#include <random>

namespace {

const auto g_bench_dir = std::filesystem::temp_directory_path() / "bench_storage";

void Setup() {
  std::error_code ec;
  std::filesystem::create_directories(g_bench_dir, ec);
}

void Cleanup() {
  std::error_code ec;
  std::filesystem::remove_all(g_bench_dir, ec);
}

} // namespace

// ============================================================================
// PAGE BENCHMARKS
// ============================================================================

static void BM_Page_ComputeChecksum(benchmark::State& state) {
  core_engine::Page page;

  // Fill page with random data
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(0, 255);
  for (size_t i = 0; i < core_engine::Page::DataSize(); i++) {
    page.GetData()[i] = static_cast<char>(dist(rng));
  }

  for (auto _ : state) {
    auto checksum = page.ComputeChecksum();
    benchmark::DoNotOptimize(checksum);
  }

  state.SetBytesProcessed(state.iterations() * core_engine::kPageSize);
}
BENCHMARK(BM_Page_ComputeChecksum);

static void BM_Page_VerifyChecksum(benchmark::State& state) {
  core_engine::Page page;
  page.SetPageId(1);
  page.SetLSN(12345);
  page.UpdateChecksum();

  for (auto _ : state) {
    bool valid = page.VerifyChecksum();
    benchmark::DoNotOptimize(valid);
  }

  state.SetBytesProcessed(state.iterations() * core_engine::kPageSize);
}
BENCHMARK(BM_Page_VerifyChecksum);

// ============================================================================
// DISK MANAGER BENCHMARKS
// ============================================================================

static void BM_DiskManager_SequentialWrite(benchmark::State& state) {
  Setup();

  auto db_file = g_bench_dir / "seq_write.db";
  core_engine::DiskManager dm(db_file);

  if (!dm.Open().ok()) {
    state.SkipWithError("Failed to open DiskManager");
    return;
  }

  core_engine::Page page;
  page.SetPageId(1);
  page.UpdateChecksum();

  core_engine::PageId page_id = 0;

  for (auto _ : state) {
    page_id = dm.AllocatePage();
    auto status = dm.WritePage(page_id, &page);
    if (!status.ok()) {
      state.SkipWithError("Write failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * core_engine::kPageSize);

  dm.Close();
  Cleanup();
}
BENCHMARK(BM_DiskManager_SequentialWrite);

static void BM_DiskManager_RandomRead(benchmark::State& state) {
  Setup();

  auto db_file = g_bench_dir / "random_read.db";
  core_engine::DiskManager dm(db_file);

  if (!dm.Open().ok()) {
    state.SkipWithError("Failed to open DiskManager");
    return;
  }

  // Pre-populate 1000 pages
  core_engine::Page write_page;
  for (int i = 1; i <= 1000; i++) {
    write_page.SetPageId(i);
    write_page.UpdateChecksum();
    auto page_id = dm.AllocatePage();
    dm.WritePage(page_id, &write_page);
  }

  std::mt19937 rng(42);
  std::uniform_int_distribution<core_engine::PageId> dist(1, 1000);

  core_engine::Page read_page;

  for (auto _ : state) {
    auto page_id = dist(rng);
    auto status = dm.ReadPage(page_id, &read_page);
    if (!status.ok()) {
      state.SkipWithError("Read failed");
      return;
    }
    benchmark::DoNotOptimize(read_page);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * core_engine::kPageSize);

  dm.Close();
  Cleanup();
}
BENCHMARK(BM_DiskManager_RandomRead);

// ============================================================================
// BUFFER POOL MANAGER BENCHMARKS
// ============================================================================

static void BM_BufferPool_CacheHit(benchmark::State& state) {
  Setup();

  auto db_file = g_bench_dir / "cache_hit.db";
  core_engine::DiskManager dm(db_file);

  if (!dm.Open().ok()) {
    state.SkipWithError("Failed to open DiskManager");
    return;
  }

  const size_t pool_size = 128; // 128 pages = 512 KB
  core_engine::BufferPoolManager bpm(pool_size, &dm);

  // Pre-populate 10 pages
  std::vector<core_engine::PageId> page_ids;
  for (int i = 0; i < 10; i++) {
    core_engine::PageId page_id;
    auto* page = bpm.NewPage(&page_id);
    if (page) {
      bpm.UnpinPage(page_id, true);
      page_ids.push_back(page_id);
    }
  }

  // Benchmark cache hits (repeatedly fetch same pages)
  size_t idx = 0;
  for (auto _ : state) {
    auto page_id = page_ids[idx % page_ids.size()];
    auto* page = bpm.FetchPage(page_id);
    if (!page) {
      state.SkipWithError("FetchPage failed");
      return;
    }
    bpm.UnpinPage(page_id, false);
    idx++;
  }

  state.SetItemsProcessed(state.iterations());

  dm.Close();
  Cleanup();
}
BENCHMARK(BM_BufferPool_CacheHit);

static void BM_BufferPool_CacheMiss(benchmark::State& state) {
  Setup();

  auto db_file = g_bench_dir / "cache_miss.db";
  core_engine::DiskManager dm(db_file);

  if (!dm.Open().ok()) {
    state.SkipWithError("Failed to open DiskManager");
    return;
  }

  const size_t pool_size = 64; // Small pool to force evictions
  core_engine::BufferPoolManager bpm(pool_size, &dm);

  // Pre-populate 200 pages (will overflow pool)
  std::vector<core_engine::PageId> page_ids;
  for (int i = 0; i < 200; i++) {
    core_engine::PageId page_id;
    auto* page = bpm.NewPage(&page_id);
    if (page) {
      bpm.UnpinPage(page_id, true);
      page_ids.push_back(page_id);
    }
  }

  bpm.FlushAllPages();

  // Benchmark cache misses (fetch pages that aren't in pool)
  std::mt19937 rng(42);
  std::uniform_int_distribution<size_t> dist(0, page_ids.size() - 1);

  for (auto _ : state) {
    auto page_id = page_ids[dist(rng)];
    auto* page = bpm.FetchPage(page_id);
    if (!page) {
      state.SkipWithError("FetchPage failed");
      return;
    }
    bpm.UnpinPage(page_id, false);
  }

  state.SetItemsProcessed(state.iterations());

  dm.Close();
  Cleanup();
}
BENCHMARK(BM_BufferPool_CacheMiss);

static void BM_BufferPool_PinUnpin(benchmark::State& state) {
  Setup();

  auto db_file = g_bench_dir / "pin_unpin.db";
  core_engine::DiskManager dm(db_file);

  if (!dm.Open().ok()) {
    state.SkipWithError("Failed to open DiskManager");
    return;
  }

  core_engine::BufferPoolManager bpm(128, &dm);

  core_engine::PageId page_id;
  auto* page = bpm.NewPage(&page_id);
  if (!page) {
    state.SkipWithError("Failed to allocate page");
    return;
  }
  bpm.UnpinPage(page_id, false);

  for (auto _ : state) {
    auto* p = bpm.FetchPage(page_id); // Pin
    benchmark::DoNotOptimize(p);
    bpm.UnpinPage(page_id, false); // Unpin
  }

  state.SetItemsProcessed(state.iterations() * 2); // pin + unpin

  dm.Close();
  Cleanup();
}
BENCHMARK(BM_BufferPool_PinUnpin);

BENCHMARK_MAIN();
