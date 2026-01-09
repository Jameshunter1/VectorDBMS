#include <benchmark/benchmark.h>

#include <core_engine/engine.hpp>
#include <core_engine/metrics.hpp>
#include <core_engine/rate_limiter.hpp>

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
const auto g_bench_dir = std::filesystem::temp_directory_path() / "core_engine_bench_advanced";

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
// v1.4: BATCH OPERATIONS BENCHMARKS
// ============================================================================

static void BM_BatchWrite_Small(benchmark::State& state) {
  SetupBenchDir();

  core_engine::Engine engine;
  engine.Open(g_bench_dir / "bench_batch_write_small");

  std::mt19937 rng(42);
  const int batch_size = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<core_engine::Engine::BatchOperation> ops;
    for (int i = 0; i < batch_size; i++) {
      auto [key, value] = GenerateKV(rng, 100);
      ops.push_back({core_engine::Engine::BatchOperation::Type::kPut, key, value});
    }
    state.ResumeTiming();

    auto status = engine.BatchWrite(ops);
    if (!status.ok()) {
      state.SkipWithError("Batch write failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations() * batch_size);
  state.SetBytesProcessed(state.iterations() * batch_size * 110); // key + value

  CleanupBenchDir();
}
BENCHMARK(BM_BatchWrite_Small)->Arg(10)->Arg(50)->Arg(100)->Arg(500)->Arg(1000);

static void BM_BatchGet(benchmark::State& state) {
  SetupBenchDir();

  core_engine::Engine engine;
  engine.Open(g_bench_dir / "bench_batch_get");

  std::mt19937 rng(42);

  // Pre-populate with 10,000 entries
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; i++) {
    auto [key, value] = GenerateKV(rng);
    engine.Put(key, value);
    if (i % 10 == 0)
      keys.push_back(key);
  }

  const int batch_size = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<std::string> batch_keys;
    for (int i = 0; i < batch_size; i++) {
      batch_keys.push_back(keys[i % keys.size()]);
    }
    state.ResumeTiming();

    auto results = engine.BatchGet(batch_keys);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations() * batch_size);

  CleanupBenchDir();
}
BENCHMARK(BM_BatchGet)->Arg(10)->Arg(50)->Arg(100)->Arg(500);

// ============================================================================
// v1.4: RANGE SCAN BENCHMARKS
// ============================================================================

static void BM_RangeScan(benchmark::State& state) {
  SetupBenchDir();

  core_engine::Engine engine;
  engine.Open(g_bench_dir / "bench_range_scan");

  // Populate with 10,000 sorted keys: key_00000 to key_09999
  for (int i = 0; i < 10000; i++) {
    std::string key = "key_" + std::to_string(10000 + i); // Ensures sorted order
    engine.Put(key, "value_" + std::to_string(i));
  }

  const int range_size = state.range(0);

  for (auto _ : state) {
    int start = 10000 + (rand() % (10000 - range_size));
    std::string start_key = "key_" + std::to_string(start);
    std::string end_key = "key_" + std::to_string(start + range_size);

    auto results = engine.Scan(start_key, end_key);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations() * range_size);

  CleanupBenchDir();
}
BENCHMARK(BM_RangeScan)->Arg(10)->Arg(100)->Arg(1000)->Arg(5000);

static void BM_RangeScan_WithLimit(benchmark::State& state) {
  SetupBenchDir();

  core_engine::Engine engine;
  engine.Open(g_bench_dir / "bench_range_scan_limit");

  // Populate with 10,000 sorted keys
  for (int i = 0; i < 10000; i++) {
    std::string key = "key_" + std::to_string(10000 + i);
    engine.Put(key, "value_" + std::to_string(i));
  }

  core_engine::ScanOptions options;
  options.limit = state.range(0);

  for (auto _ : state) {
    auto results = engine.Scan("key_10000", "key_99999", options);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations() * state.range(0));

  CleanupBenchDir();
}
BENCHMARK(BM_RangeScan_WithLimit)->Arg(10)->Arg(50)->Arg(100)->Arg(500);

static void BM_RangeScan_KeysOnly(benchmark::State& state) {
  SetupBenchDir();

  core_engine::Engine engine;
  engine.Open(g_bench_dir / "bench_range_scan_keys");

  // Populate with 5,000 entries
  for (int i = 0; i < 5000; i++) {
    std::string key = "key_" + std::to_string(10000 + i);
    engine.Put(key, std::string(100, 'x')); // 100-byte values
  }

  core_engine::ScanOptions options;
  options.keys_only = true;
  options.limit = 1000;

  for (auto _ : state) {
    auto results = engine.Scan("key_10000", "key_99999", options);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations() * 1000);

  CleanupBenchDir();
}
BENCHMARK(BM_RangeScan_KeysOnly);

// ============================================================================
// v1.4: RATE LIMITER BENCHMARKS
// ============================================================================

static void BM_RateLimiter_Allow(benchmark::State& state) {
  core_engine::RateLimiter limiter(1000.0, 2000.0); // 1000/sec, burst 2000

  int client_id = 0;
  for (auto _ : state) {
    bool allowed = limiter.Allow("client_" + std::to_string(client_id % 10));
    benchmark::DoNotOptimize(allowed);
    ++client_id;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RateLimiter_Allow);

static void BM_RateLimiterMiddleware(benchmark::State& state) {
  core_engine::RateLimiterMiddleware middleware;
  middleware.ConfigureEndpoint("/api/put", 1000.0, 2000.0);
  middleware.ConfigureEndpoint("/api/get", 5000.0, 10000.0);

  int request_id = 0;
  for (auto _ : state) {
    std::string endpoint = (request_id % 2 == 0) ? "/api/put" : "/api/get";
    std::string client = "client_" + std::to_string(request_id % 20);

    bool allowed = middleware.AllowRequest(endpoint, client);
    benchmark::DoNotOptimize(allowed);
    ++request_id;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RateLimiterMiddleware);

// ============================================================================
// v1.4: METRICS COLLECTION BENCHMARKS
// ============================================================================

static void BM_MetricsCollector_Counter(benchmark::State& state) {
  core_engine::MetricsCollector metrics;

  for (auto _ : state) {
    metrics.IncrementCounter("test_counter", 1.0);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MetricsCollector_Counter);

static void BM_MetricsCollector_Gauge(benchmark::State& state) {
  core_engine::MetricsCollector metrics;

  double value = 100.0;
  for (auto _ : state) {
    metrics.SetGauge("test_gauge", value++);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MetricsCollector_Gauge);

static void BM_MetricsCollector_Histogram(benchmark::State& state) {
  core_engine::MetricsCollector metrics;

  std::mt19937 rng(42);
  std::uniform_real_distribution<double> dist(0.0, 1.0);

  for (auto _ : state) {
    metrics.ObserveHistogram("core_engine_get_latency_seconds", dist(rng));
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MetricsCollector_Histogram);

static void BM_PrometheusExport(benchmark::State& state) {
  core_engine::MetricsCollector metrics;

  // Pre-populate with metrics
  for (int i = 0; i < 100; i++) {
    metrics.IncrementCounter("counter_" + std::to_string(i), 100.0);
    metrics.SetGauge("gauge_" + std::to_string(i), 42.0);
  }

  for (auto _ : state) {
    std::string text = metrics.GetPrometheusText();
    benchmark::DoNotOptimize(text);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PrometheusExport);

// ============================================================================
// v1.4: COMPARISON BENCHMARKS (Individual vs Batch)
// ============================================================================

static void BM_Individual_Puts_100(benchmark::State& state) {
  SetupBenchDir();

  core_engine::Engine engine;
  engine.Open(g_bench_dir / "bench_individual_puts");

  std::mt19937 rng(42);

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<std::pair<std::string, std::string>> kvs;
    for (int i = 0; i < 100; i++) {
      kvs.push_back(GenerateKV(rng));
    }
    state.ResumeTiming();

    for (const auto& [key, value] : kvs) {
      engine.Put(key, value);
    }
  }

  state.SetItemsProcessed(state.iterations() * 100);

  CleanupBenchDir();
}
BENCHMARK(BM_Individual_Puts_100);

static void BM_Batch_Puts_100(benchmark::State& state) {
  SetupBenchDir();

  core_engine::Engine engine;
  engine.Open(g_bench_dir / "bench_batch_puts");

  std::mt19937 rng(42);

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<core_engine::Engine::BatchOperation> ops;
    for (int i = 0; i < 100; i++) {
      auto [key, value] = GenerateKV(rng);
      ops.push_back({core_engine::Engine::BatchOperation::Type::kPut, key, value});
    }
    state.ResumeTiming();

    engine.BatchWrite(ops);
  }

  state.SetItemsProcessed(state.iterations() * 100);

  CleanupBenchDir();
}
BENCHMARK(BM_Batch_Puts_100);
