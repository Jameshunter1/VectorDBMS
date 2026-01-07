#include <benchmark/benchmark.h>

#include <core_engine/common/config.hpp>
#include <core_engine/engine.hpp>
#include <core_engine/vector/hnsw_index.hpp>
#include <core_engine/vector/vector.hpp>

#include <filesystem>
#include <random>

namespace {

const auto g_bench_dir = std::filesystem::temp_directory_path() / "bench_vector";

void Setup() {
  std::error_code ec;
  std::filesystem::create_directories(g_bench_dir, ec);
}

void Cleanup() {
  std::error_code ec;
  std::filesystem::remove_all(g_bench_dir, ec);
}

// Generate random vector
core_engine::vector::Vector GenerateRandomVector(size_t dimension, std::mt19937& rng) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  std::vector<float> data(dimension);
  for (size_t i = 0; i < dimension; i++) {
    data[i] = dist(rng);
  }
  return core_engine::vector::Vector(std::move(data));
}

} // namespace

// ============================================================================
// VECTOR DISTANCE BENCHMARKS
// ============================================================================

static void BM_Vector_CosineDistance_128D(benchmark::State& state) {
  std::mt19937 rng(42);
  auto v1 = GenerateRandomVector(128, rng);
  auto v2 = GenerateRandomVector(128, rng);

  for (auto _ : state) {
    float dist = core_engine::vector::CosineDistance(v1, v2);
    benchmark::DoNotOptimize(dist);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Vector_CosineDistance_128D);

static void BM_Vector_EuclideanDistance_128D(benchmark::State& state) {
  std::mt19937 rng(42);
  auto v1 = GenerateRandomVector(128, rng);
  auto v2 = GenerateRandomVector(128, rng);

  for (auto _ : state) {
    float dist = core_engine::vector::EuclideanDistance(v1, v2);
    benchmark::DoNotOptimize(dist);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Vector_EuclideanDistance_128D);

static void BM_Vector_DotProduct_128D(benchmark::State& state) {
  std::mt19937 rng(42);
  auto v1 = GenerateRandomVector(128, rng);
  auto v2 = GenerateRandomVector(128, rng);

  for (auto _ : state) {
    float dot = core_engine::vector::DotProduct(v1, v2);
    benchmark::DoNotOptimize(dot);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Vector_DotProduct_128D);

// Varying dimensions
static void BM_Vector_Cosine_VaryingDimensions(benchmark::State& state) {
  std::mt19937 rng(42);
  const size_t dim = state.range(0);
  auto v1 = GenerateRandomVector(dim, rng);
  auto v2 = GenerateRandomVector(dim, rng);

  for (auto _ : state) {
    float dist = core_engine::vector::CosineDistance(v1, v2);
    benchmark::DoNotOptimize(dist);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetLabel(std::to_string(dim) + "D");
}
BENCHMARK(BM_Vector_Cosine_VaryingDimensions)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Arg(512)
    ->Arg(1024)
    ->Arg(2048);

// ============================================================================
// HNSW INDEX BENCHMARKS
// ============================================================================

static void BM_HNSW_Insert(benchmark::State& state) {
  std::mt19937 rng(42);

  core_engine::vector::HNSWIndex::Params params;
  params.dimension = 128;
  params.metric = core_engine::vector::DistanceMetric::kCosine;
  params.M = 16;
  params.ef_construction = 200;

  core_engine::vector::HNSWIndex index(params);

  // Pre-populate with some vectors
  for (int i = 0; i < 1000; i++) {
    auto vec = GenerateRandomVector(128, rng);
    index.Insert("doc_" + std::to_string(i), vec);
  }

  int counter = 1000;

  for (auto _ : state) {
    auto vec = GenerateRandomVector(128, rng);
    auto status = index.Insert("doc_" + std::to_string(counter++), vec);
    if (!status.ok()) {
      state.SkipWithError("Insert failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HNSW_Insert);

static void BM_HNSW_Search_K10(benchmark::State& state) {
  std::mt19937 rng(42);

  core_engine::vector::HNSWIndex::Params params;
  params.dimension = 128;
  params.metric = core_engine::vector::DistanceMetric::kCosine;
  params.M = 16;
  params.ef_construction = 200;
  params.ef_search = 50;

  core_engine::vector::HNSWIndex index(params);

  // Pre-populate with 10,000 vectors
  for (int i = 0; i < 10000; i++) {
    auto vec = GenerateRandomVector(128, rng);
    index.Insert("doc_" + std::to_string(i), vec);
  }

  for (auto _ : state) {
    auto query = GenerateRandomVector(128, rng);
    auto results = index.Search(query, 10);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HNSW_Search_K10);

static void BM_HNSW_Search_VaryingK(benchmark::State& state) {
  std::mt19937 rng(42);

  core_engine::vector::HNSWIndex::Params params;
  params.dimension = 128;
  params.metric = core_engine::vector::DistanceMetric::kCosine;
  params.M = 16;
  params.ef_construction = 200;
  params.ef_search = 50;

  core_engine::vector::HNSWIndex index(params);

  // Pre-populate
  for (int i = 0; i < 10000; i++) {
    auto vec = GenerateRandomVector(128, rng);
    index.Insert("doc_" + std::to_string(i), vec);
  }

  const size_t k = state.range(0);

  for (auto _ : state) {
    auto query = GenerateRandomVector(128, rng);
    auto results = index.Search(query, k);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetLabel("k=" + std::to_string(k));
}
BENCHMARK(BM_HNSW_Search_VaryingK)->Arg(1)->Arg(5)->Arg(10)->Arg(20)->Arg(50)->Arg(100);

static void BM_HNSW_Search_VaryingIndexSize(benchmark::State& state) {
  std::mt19937 rng(42);

  core_engine::vector::HNSWIndex::Params params;
  params.dimension = 128;
  params.metric = core_engine::vector::DistanceMetric::kCosine;
  params.M = 16;
  params.ef_construction = 200;
  params.ef_search = 50;

  core_engine::vector::HNSWIndex index(params);

  const int index_size = state.range(0);

  // Pre-populate
  for (int i = 0; i < index_size; i++) {
    auto vec = GenerateRandomVector(128, rng);
    index.Insert("doc_" + std::to_string(i), vec);
  }

  for (auto _ : state) {
    auto query = GenerateRandomVector(128, rng);
    auto results = index.Search(query, 10);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetLabel(std::to_string(index_size) + " vectors");
}
BENCHMARK(BM_HNSW_Search_VaryingIndexSize)
    ->Arg(1000)
    ->Arg(5000)
    ->Arg(10000)
    ->Arg(50000)
    ->Arg(100000);

// ============================================================================
// ENGINE VECTOR OPERATIONS BENCHMARKS
// ============================================================================

static void BM_Engine_PutVector(benchmark::State& state) {
  Setup();

  core_engine::DatabaseConfig config =
      core_engine::DatabaseConfig::Embedded(g_bench_dir / "put_vector");
  config.enable_vector_index = true;
  config.vector_dimension = 128;
  config.vector_metric = core_engine::DatabaseConfig::VectorDistanceMetric::kCosine;
  config.hnsw_params.M = 16;
  config.hnsw_params.ef_construction = 200;

  core_engine::Engine engine;
  auto status = engine.Open(config);
  if (!status.ok()) {
    state.SkipWithError("Failed to open engine");
    return;
  }

  std::mt19937 rng(42);
  int counter = 0;

  for (auto _ : state) {
    auto vec = GenerateRandomVector(128, rng);
    auto put_status = engine.PutVector("vec_" + std::to_string(counter++), vec);
    if (!put_status.ok()) {
      state.SkipWithError("PutVector failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations());

  Cleanup();
}
BENCHMARK(BM_Engine_PutVector);

static void BM_Engine_SearchSimilar(benchmark::State& state) {
  Setup();

  core_engine::DatabaseConfig config =
      core_engine::DatabaseConfig::Embedded(g_bench_dir / "search_similar");
  config.enable_vector_index = true;
  config.vector_dimension = 128;
  config.vector_metric = core_engine::DatabaseConfig::VectorDistanceMetric::kCosine;
  config.hnsw_params.M = 16;
  config.hnsw_params.ef_construction = 200;
  config.hnsw_params.ef_search = 50;

  core_engine::Engine engine;
  auto status = engine.Open(config);
  if (!status.ok()) {
    state.SkipWithError("Failed to open engine");
    return;
  }

  std::mt19937 rng(42);

  // Pre-populate with 10,000 vectors
  for (int i = 0; i < 10000; i++) {
    auto vec = GenerateRandomVector(128, rng);
    engine.PutVector("vec_" + std::to_string(i), vec);
  }

  for (auto _ : state) {
    auto query = GenerateRandomVector(128, rng);
    auto results = engine.SearchSimilar(query, 10);
    benchmark::DoNotOptimize(results);
  }

  state.SetItemsProcessed(state.iterations());

  Cleanup();
}
BENCHMARK(BM_Engine_SearchSimilar);
