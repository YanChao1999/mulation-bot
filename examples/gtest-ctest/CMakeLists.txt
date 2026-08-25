# Example: Google Test + CTest + mulation
#
# ctest                 -> unit tests (must pass)
# mulation-check        -> mutation gate (prove those tests would catch bugs)

if(NOT TARGET mulation_plugin)
  cmake_minimum_required(VERSION 3.16)
  project(mulation-gtest-example LANGUAGES C CXX)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
  include(Mulation)
endif()

find_package(GTest QUIET)
if(GTest_FOUND)
  set(MULATION_EXAMPLE_GTEST ON)
else()
  set(MULATION_EXAMPLE_GTEST OFF)
  message(STATUS "GTest not found; using bundled mini_gtest.h (same TEST/EXPECT_* macros)")
endif()

add_library(compare STATIC src/compare.cpp)
target_include_directories(compare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
mulation_instrument(compare)

add_executable(compare_tests tests/compare_test.cpp)
target_include_directories(compare_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(compare_tests PRIVATE compare)
if(MULATION_EXAMPLE_GTEST)
  target_compile_definitions(compare_tests PRIVATE MULATION_USE_GTEST=1)
  target_link_libraries(compare_tests PRIVATE GTest::gtest_main)
endif()

enable_testing()
add_test(NAME compare_tests COMMAND compare_tests)

mulation_add_check(compare_tests MIN_SCORE ${MULATION_MIN_SCORE})
