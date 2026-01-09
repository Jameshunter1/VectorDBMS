cmake_minimum_required(VERSION 3.24)

# BENCHMARKS
#
# Google Benchmark integration with Windows pthread.h workaround

include(FetchContent)

# Google Benchmark configuration
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Windows-specific configuration to prevent pthread.h errors
if(WIN32)
  # Disable all POSIX-specific features
  set(HAVE_POSIX_REGEX 0 CACHE INTERNAL "" FORCE)
  set(HAVE_PTHREAD_AFFINITY 0 CACHE INTERNAL "" FORCE)
  set(BENCHMARK_ENABLE_LIBPFM OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_USE_LIBCXX OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_PTHREAD OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_ASSEMBLY_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_LTO OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.5
)

FetchContent_MakeAvailable(benchmark)

# Comprehensive patch for Google Benchmark's sysinfo.cc on Windows
# This patches the ThreadAffinityGuard class to guard all POSIX pthread code
if(WIN32)
  FetchContent_GetProperties(benchmark)
  set(SYSINFO_FILE "${benchmark_SOURCE_DIR}/src/sysinfo.cc")
  
  if(EXISTS "${SYSINFO_FILE}")
    file(READ "${SYSINFO_FILE}" SYSINFO_CONTENT)
    
    # Check if already patched
    string(FIND "${SYSINFO_CONTENT}" "// PATCHED FOR WINDOWS" PATCH_POS)
    
    if(PATCH_POS EQUAL -1)
      # Patch 1: Guard pthread.h include (under BENCHMARK_HAS_PTHREAD_AFFINITY)
      string(REPLACE 
        "#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY)\n#include <pthread.h>"
        "// PATCHED FOR WINDOWS: Guard POSIX-specific pthread APIs\n#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY) && (defined(__unix__) || defined(__APPLE__))\n#include <pthread.h>"
        SYSINFO_CONTENT 
        "${SYSINFO_CONTENT}"
      )
      
      # Patch 2: Guard the SetAffinity() pthread implementation
      # Find the pthread_getaffinity_np section and wrap it
      string(REPLACE
        "  bool SetAffinity() {\n#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY)\n    int ret;\n    self = pthread_self();"
        "  bool SetAffinity() {\n#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY) && (defined(__unix__) || defined(__APPLE__))\n    int ret;\n    self = pthread_self();"
        SYSINFO_CONTENT
        "${SYSINFO_CONTENT}"
      )
      
      # Patch 3: Guard pthread member variables in ThreadAffinityGuard
      string(REPLACE
        "#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY)\n  pthread_t self;\n  cpu_set_t previous_affinity;"
        "#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY) && (defined(__unix__) || defined(__APPLE__))\n  pthread_t self;\n  cpu_set_t previous_affinity;"
        SYSINFO_CONTENT
        "${SYSINFO_CONTENT}"
      )
      
      # Patch 4: Guard the destructor's pthread_setaffinity_np call
      string(REPLACE
        "#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY)\n    int ret = pthread_setaffinity_np(self, sizeof(previous_affinity),\n                                     &previous_affinity);"
        "#if defined(BENCHMARK_HAS_PTHREAD_AFFINITY) && (defined(__unix__) || defined(__APPLE__))\n    int ret = pthread_setaffinity_np(self, sizeof(previous_affinity),\n                                     &previous_affinity);"
        SYSINFO_CONTENT
        "${SYSINFO_CONTENT}"
      )
      
      file(WRITE "${SYSINFO_FILE}" "${SYSINFO_CONTENT}")
      message(STATUS "Patched ${SYSINFO_FILE} to guard all POSIX pthread APIs with platform checks")
    else()
      message(STATUS "Skipping patch: ${SYSINFO_FILE} already patched")
    endif()
  else()
    message(WARNING "Could not find ${SYSINFO_FILE} to patch")
  endif()
endif()

# Build benchmarks executable
add_executable(core_engine_benchmarks
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
