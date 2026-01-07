cmake_minimum_required(VERSION 3.24)

# TEST-SUITE
#
# This CMakeLists is added by CORE-ENGINE via add_subdirectory(../tests ...).
# It assumes the target core_engine::core_engine already exists.

include(FetchContent)

# Keep test dependencies pinned for reproducibility.
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

add_executable(core_engine_tests
  test_engine.cpp
  test_web_api.cpp
  test_storage_layer.cpp
)
set_target_properties(core_engine_tests PROPERTIES FOLDER "tests")

target_link_libraries(core_engine_tests
  PRIVATE
    core_engine::core_engine
    Catch2::Catch2WithMain
    httplib::httplib
)

# Security tests (standalone executable)
add_executable(security_tests
  test_security.cpp
)
set_target_properties(security_tests PROPERTIES FOLDER "tests")

target_link_libraries(security_tests
  PRIVATE
    core_engine::core_engine
)

# Integration security tests (standalone executable)
add_executable(integration_tests
  test_integration_security.cpp
)
set_target_properties(integration_tests PROPERTIES FOLDER "tests")

target_link_libraries(integration_tests
  PRIVATE
    core_engine::core_engine
)

# v1.4 Advanced features tests - batch ops, range queries, rate limiting, metrics
add_executable(advanced_tests
  test_advanced_features.cpp
)
set_target_properties(advanced_tests PROPERTIES FOLDER "tests")

target_link_libraries(advanced_tests
  PRIVATE
    core_engine::core_engine
)

target_compile_features(core_engine_tests PRIVATE cxx_std_20)
target_compile_features(security_tests PRIVATE cxx_std_20)
target_compile_features(integration_tests PRIVATE cxx_std_20)
target_compile_features(advanced_tests PRIVATE cxx_std_20)

include(CTest)
include(Catch)
catch_discover_tests(core_engine_tests)
