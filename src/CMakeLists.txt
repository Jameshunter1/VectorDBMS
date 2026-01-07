cmake_minimum_required(VERSION 3.24)

# Core-Engine (database engine) top-level CMake project.
#
# Why this is the "project root":
# - In your VS Code multi-root workspace, CORE-ENGINE maps to ./src.
# - Tests and benchmarks live in sibling folders (../tests, ../benchmarks).
# - This layout keeps the engine itself clean and allows independent tooling.

project(core_engine
  VERSION 0.1.0
  DESCRIPTION "C++ database engine core"
  LANGUAGES CXX
)

# ---- Global policy / defaults ------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export a compilation database when supported by the generator.
# This is helpful for IDEs, static analysis, and code navigation tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Favor folder grouping in IDEs.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---- Options (keep stable over years) ---------------------------------------
option(CORE_ENGINE_BUILD_TESTS "Build unit tests" ON)
option(CORE_ENGINE_BUILD_BENCHMARKS "Build benchmarks" ON)
option(CORE_ENGINE_BUILD_WEB "Build minimal web frontend (dbweb)" ON)
option(CORE_ENGINE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CORE_ENGINE_ENABLE_SANITIZERS "Enable sanitizer flags (best on Clang/GCC)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ProjectOptions)

# ---- Core library ------------------------------------------------------------
add_library(core_engine)
add_library(core_engine::core_engine ALIAS core_engine)

# Public headers
target_include_directories(core_engine
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

# Implementation sources
target_sources(core_engine
  PRIVATE
    lib/common/config.cpp
    lib/common/status.cpp
    lib/common/logger.cpp
    lib/catalog/catalog.cpp
    lib/storage/page.cpp
    lib/storage/disk_manager.cpp
    lib/transaction/txn.cpp
    lib/execution/executor.cpp
    lib/engine.cpp
    lib/security/auth.cpp
    lib/security/audit.cpp
    lib/config/app_config.cpp
    lib/performance/rate_limiter.cpp
    lib/performance/metrics.cpp
    lib/vector/vector.cpp
    lib/vector/hnsw_index.cpp
)

target_compile_features(core_engine PUBLIC cxx_std_20)

core_engine_apply_project_options(core_engine)
core_engine_apply_warnings(core_engine ${CORE_ENGINE_WARNINGS_AS_ERRORS})
core_engine_apply_sanitizers(core_engine ${CORE_ENGINE_ENABLE_SANITIZERS})

# ---- CLI (developer utility) -------------------------------------------------
# A tiny executable that proves the library links and can evolve into admin tools.
add_executable(dbcli apps/dbcli/main.cpp)
set_target_properties(dbcli PROPERTIES OUTPUT_NAME "dbcli")
target_link_libraries(dbcli PRIVATE core_engine::core_engine)
core_engine_apply_project_options(dbcli)
core_engine_apply_warnings(dbcli ${CORE_ENGINE_WARNINGS_AS_ERRORS})
core_engine_apply_sanitizers(dbcli ${CORE_ENGINE_ENABLE_SANITIZERS})

# ---- Tutorial (v1.4 interactive demo) ---------------------------------------
add_executable(tutorial_v1.4 apps/tutorial/tutorial_v1.4.cpp)
set_target_properties(tutorial_v1.4 PROPERTIES OUTPUT_NAME "tutorial_v1.4")
target_link_libraries(tutorial_v1.4 PRIVATE core_engine::core_engine)
core_engine_apply_project_options(tutorial_v1.4)
core_engine_apply_warnings(tutorial_v1.4 ${CORE_ENGINE_WARNINGS_AS_ERRORS})
core_engine_apply_sanitizers(tutorial_v1.4 ${CORE_ENGINE_ENABLE_SANITIZERS})

# ---- Web frontend (developer demo) ------------------------------------------
if(CORE_ENGINE_BUILD_WEB)
  include(FetchContent)

  # cpp-httplib: small single-header HTTP server/client library.
  # Used here to provide a visible browser UI without adding a heavy framework.
  FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3  # Use the latest stable version
  )
  FetchContent_MakeAvailable(httplib)

  add_executable(dbweb apps/dbweb/main.cpp)
  set_target_properties(dbweb PROPERTIES OUTPUT_NAME "dbweb")
  target_link_libraries(dbweb PRIVATE core_engine::core_engine httplib::httplib)
  core_engine_apply_project_options(dbweb)
  core_engine_apply_warnings(dbweb ${CORE_ENGINE_WARNINGS_AS_ERRORS})
  core_engine_apply_sanitizers(dbweb ${CORE_ENGINE_ENABLE_SANITIZERS})
endif()

# ---- Tests / Benchmarks ------------------------------------------------------
include(CTest)

if(CORE_ENGINE_BUILD_TESTS)
  # We keep tests in a sibling folder to match your workspace layout.
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../tests" "${CMAKE_BINARY_DIR}/tests")
endif()

if(CORE_ENGINE_BUILD_BENCHMARKS)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../benchmarks" "${CMAKE_BINARY_DIR}/benchmarks")
endif()
