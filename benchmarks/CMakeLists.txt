cmake_minimum_required(VERSION 3.24)

# BENCHMARKS
#
# This CMakeLists is added by CORE-ENGINE via add_subdirectory(../benchmarks ...).
# It assumes the target core_engine::core_engine already exists.

include(FetchContent)

# Google Benchmark uses GoogleTest for some targets; disable tests to keep builds lean.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# On Windows, Google Benchmark will detect missing POSIX features (pthreads, shm_open, etc.)
# This is expected behavior and should not cause build failures.
# Set these BEFORE FetchContent_Declare to ensure they're used during configuration.
if(WIN32)
  # Disable POSIX-specific features that are not available on Windows
  set(HAVE_POSIX_REGEX 0 CACHE INTERNAL "" FORCE)
  set(HAVE_PTHREAD_AFFINITY 0 CACHE INTERNAL "" FORCE)
  set(BENCHMARK_ENABLE_LIBPFM OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_USE_LIBCXX OFF CACHE BOOL "" FORCE)
  
  # CRITICAL: Disable pthread usage completely on Windows
  set(BENCHMARK_ENABLE_PTHREAD OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_ASSEMBLY_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_LTO OFF CACHE BOOL "" FORCE)
  
  # Add preprocessor definitions that will propagate to all targets
  add_compile_definitions(BENCHMARK_OS_WINDOWS=1)
  add_compile_definitions(_WIN32=1)
  
  # Ensure these are set in CMAKE_CXX_FLAGS so they're used during feature detection
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBENCHMARK_OS_WINDOWS=1" CACHE STRING "" FORCE)
endif()

FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.5
)

FetchContent_MakeAvailable(benchmark)

add_executable(core_engine_benchmarks
  bench_page_file.cpp
  bench_advanced.cpp
  bench_storage_layer.cpp
  bench_vector_ops.cpp
)
set_target_properties(core_engine_benchmarks PROPERTIES FOLDER "benchmarks")

target_link_libraries(core_engine_benchmarks
  PRIVATE
    core_engine::core_engine
    benchmark::benchmark
)

target_compile_features(core_engine_benchmarks PRIVATE cxx_std_20)
