#include <benchmark/benchmark.h>

#include <core_engine/storage/page_file.hpp>

#include <filesystem>

static void BM_PageFileWrite(benchmark::State& state) {
  const auto db_dir = std::filesystem::temp_directory_path() / "core_engine_bench_db";
  std::filesystem::create_directories(db_dir);

  core_engine::PageFile file(db_dir / "bench.pages");
  auto status = file.OpenOrCreate();
  if (!status.ok()) {
    state.SkipWithError(status.ToString().c_str());
    return;
  }

  core_engine::Page page;

  for (auto _ : state) {
    status = file.Write(0, page);
    if (!status.ok()) {
      state.SkipWithError(status.ToString().c_str());
      return;
    }
  }
}

BENCHMARK(BM_PageFileWrite);

BENCHMARK_MAIN();
