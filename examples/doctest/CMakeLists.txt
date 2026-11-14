# Example: doctest + mulation
#
# compare_tests         -> unit tests (must pass)
# mulation-check        -> mutation gate

if(NOT TARGET mulation_plugin)
  cmake_minimum_required(VERSION 3.16)
  project(mulation-doctest-example LANGUAGES C CXX)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
  include(Mulation)
endif()

add_library(doctest_compare STATIC src/compare.cpp)
target_include_directories(doctest_compare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
mulation_instrument(doctest_compare)

add_executable(doctest_compare_tests tests/compare_test.cpp)
target_include_directories(doctest_compare_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(doctest_compare_tests PRIVATE doctest_compare)

enable_testing()
add_test(NAME doctest_compare_tests COMMAND doctest_compare_tests)

mulation_add_check(doctest_compare_tests MIN_SCORE ${MULATION_MIN_SCORE})
