cmake_minimum_required(VERSION 3.24)

# Vectis - Production Vector Database
# Monorepo CMake Configuration

project(VectorDBMS
  VERSION 1.5.0
  DESCRIPTION "C++20 page-oriented vector database with HNSW indexing"
  LANGUAGES CXX
)

# ---- Global settings --------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compilation database for IDEs and tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---- Project options --------------------------------------------------------
option(CORE_ENGINE_BUILD_TESTS "Build unit tests" ON)
option(CORE_ENGINE_BUILD_BENCHMARKS "Build benchmarks" ON)
option(CORE_ENGINE_BUILD_WEB "Build web server (dbweb)" ON)
option(CORE_ENGINE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CORE_ENGINE_ENABLE_SANITIZERS "Enable sanitizer flags (Clang/GCC)" OFF)

# ---- CMake modules ----------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ProjectOptions)

# ---- Core library (libcore_engine.a) ----------------------------------------
add_library(core_engine)
add_library(core_engine::core_engine ALIAS core_engine)

# Public headers
target_include_directories(core_engine
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/include>"
    "$<INSTALL_INTERFACE:include>"
)

# Implementation sources
target_sources(core_engine
  PRIVATE
    src/lib/common/config.cpp
    src/lib/common/status.cpp
    src/lib/common/logger.cpp
    src/lib/catalog/catalog.cpp
    src/lib/storage/page.cpp
    src/lib/storage/page_file.cpp
    src/lib/storage/disk_manager.cpp
    src/lib/storage/buffer_pool_manager.cpp
    src/lib/storage/log_manager.cpp
    src/lib/transaction/txn.cpp
    src/lib/execution/executor.cpp
    src/lib/engine.cpp
    src/lib/security/auth.cpp
    src/lib/security/audit.cpp
    src/lib/config/app_config.cpp
    src/lib/performance/rate_limiter.cpp
    src/lib/performance/metrics.cpp
    src/lib/vector/vector.cpp
    src/lib/vector/hnsw_index.cpp
)

target_compile_features(core_engine PUBLIC cxx_std_20)

core_engine_apply_project_options(core_engine)
core_engine_apply_warnings(core_engine ${CORE_ENGINE_WARNINGS_AS_ERRORS})
core_engine_apply_sanitizers(core_engine ${CORE_ENGINE_ENABLE_SANITIZERS})

# ---- Executables ------------------------------------------------------------

# dbcli - Command-line interface
add_executable(dbcli src/apps/dbcli/main.cpp)
set_target_properties(dbcli PROPERTIES OUTPUT_NAME "dbcli")
target_link_libraries(dbcli PRIVATE core_engine::core_engine)
core_engine_apply_project_options(dbcli)
core_engine_apply_warnings(dbcli ${CORE_ENGINE_WARNINGS_AS_ERRORS})
core_engine_apply_sanitizers(dbcli ${CORE_ENGINE_ENABLE_SANITIZERS})

# dbweb - HTTP server with web UI
if(CORE_ENGINE_BUILD_WEB)
  include(FetchContent)

  # cpp-httplib: single-header HTTP server library
  FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
  )
  FetchContent_MakeAvailable(httplib)

  add_executable(dbweb src/apps/dbweb/main.cpp)
  set_target_properties(dbweb PROPERTIES OUTPUT_NAME "dbweb")
  target_link_libraries(dbweb PRIVATE core_engine::core_engine httplib::httplib)
  core_engine_apply_project_options(dbweb)
  core_engine_apply_warnings(dbweb ${CORE_ENGINE_WARNINGS_AS_ERRORS})
  core_engine_apply_sanitizers(dbweb ${CORE_ENGINE_ENABLE_SANITIZERS})
endif()

# ---- Tests and Benchmarks ---------------------------------------------------
include(CTest)

if(CORE_ENGINE_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(CORE_ENGINE_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
