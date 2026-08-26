cmake_minimum_required(VERSION 3.16)
project(mulation-bot VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(WARNING "Build the pass plugin with Clang: cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LLVM_CONFIG "" CACHE FILEPATH "llvm-config executable")
if(NOT LLVM_CONFIG)
  find_program(LLVM_CONFIG NAMES llvm-config-18 llvm-config HINTS /usr/lib/llvm-18/bin)
endif()
if(NOT LLVM_CONFIG)
  message(FATAL_ERROR "llvm-config not found (need LLVM 18 headers to build the pass plugin)")
endif()

execute_process(COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --includedir OUTPUT_VARIABLE LLVM_INCLUDEDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --libdir OUTPUT_VARIABLE LLVM_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE)

separate_arguments(LLVM_CXXFLAGS_LIST UNIX_COMMAND "${LLVM_CXXFLAGS}")

add_library(mulation_plugin SHARED plugin/MulationPass.cpp)
target_include_directories(mulation_plugin PRIVATE ${LLVM_INCLUDEDIR})
target_compile_options(mulation_plugin PRIVATE ${LLVM_CXXFLAGS_LIST} -fPIC -fno-rtti)
target_link_directories(mulation_plugin PRIVATE ${LLVM_LIBDIR})
target_link_libraries(mulation_plugin PRIVATE LLVM)
set_target_properties(mulation_plugin PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "mulation_plugin"
  BUILD_RPATH "${LLVM_LIBDIR}"
  INSTALL_RPATH "${LLVM_LIBDIR}"
)

add_library(mulation_runtime STATIC runtime/mulation_runtime.c)
target_include_directories(mulation_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(mulation_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(mulation_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)

set(_runner_src
  runner/main.cpp
  runner/catalog.cpp
  runner/process.cpp
  runner/diff.cpp
  runner/cli.cpp
)
add_executable(mulation-run ${_runner_src})
target_include_directories(mulation-run PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runner)
target_compile_options(mulation-run PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(mulation ${_runner_src})
target_include_directories(mulation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runner)
target_compile_options(mulation PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(GNUInstallDirs)
install(TARGETS mulation_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS mulation_runtime ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS mulation-run mulation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/mulation DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES cmake/Mulation.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mulation)

set(MULATION_PLUGIN_PATH "$<TARGET_FILE:mulation_plugin>" CACHE INTERNAL "")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Mulation.cmake)

option(MULATION_BUILD_TESTS "Build tool unit tests" ON)
option(MULATION_BUILD_EXAMPLES "Build the gtest/CTest example" ON)
if(MULATION_BUILD_TESTS OR MULATION_BUILD_EXAMPLES)
  enable_testing()
endif()

if(MULATION_BUILD_TESTS)
  set(_unit_test_src
    tests/unit_main.cpp
    tests/catalog_test.cpp
    tests/diff_test.cpp
    tests/process_test.cpp
    tests/cli_test.cpp
    tests/elf_catalog_test.cpp
  )
  set(_runner_lib_src
    runner/catalog.cpp
    runner/diff.cpp
    runner/process.cpp
    runner/cli.cpp
  )
  add_executable(mulation_unit_tests ${_unit_test_src} ${_runner_lib_src})
  target_include_directories(mulation_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runner
    ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(mulation_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME mulation_unit_tests COMMAND mulation_unit_tests)

  set(_self_test_src
    tests/unit_main.cpp
    tests/catalog_test.cpp
    tests/diff_test.cpp
    tests/cli_test.cpp
    tests/elf_catalog_test.cpp
  )
  set(_self_sut_src
    runner/catalog.cpp
    runner/diff.cpp
    runner/cli.cpp
  )
  # Dogfood: instrument catalog/diff/cli only; omit process (+ its tests) for speed.
  add_library(mulation_self_sut OBJECT ${_self_sut_src})
  target_include_directories(mulation_self_sut PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runner
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_options(mulation_self_sut PRIVATE
    -Wall -Wextra -Wpedantic -Werror -g -O0
    "-fpass-plugin=${MULATION_PLUGIN_PATH}")
  if(TARGET mulation_plugin)
    add_dependencies(mulation_self_sut mulation_plugin)
  endif()
  add_executable(mulation_self_tests ${_self_test_src} runner/process.cpp
                 $<TARGET_OBJECTS:mulation_self_sut>)
  target_include_directories(mulation_self_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runner
    ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(mulation_self_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -g -O0)
  target_link_libraries(mulation_self_tests PRIVATE ${MULATION_RUNTIME_TARGET})
  target_include_directories(mulation_self_tests PRIVATE
    $<TARGET_PROPERTY:${MULATION_RUNTIME_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
  add_test(NAME mulation_self_check
    COMMAND $<TARGET_FILE:mulation-run> --min-score 0 $<TARGET_FILE:mulation_self_tests>)
  set_tests_properties(mulation_self_check PROPERTIES DEPENDS mulation_unit_tests)

  add_executable(runtime_test tests/runtime_test.c)
  target_link_libraries(runtime_test PRIVATE mulation_runtime)
  target_compile_options(runtime_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME runtime_inactive COMMAND runtime_test 1 0)
  set_tests_properties(runtime_inactive PROPERTIES ENVIRONMENT "MULATION_MUTANT=")
  add_test(NAME runtime_active_match COMMAND runtime_test 42 1)
  set_tests_properties(runtime_active_match PROPERTIES ENVIRONMENT MULATION_MUTANT=42)
  add_test(NAME runtime_active_miss COMMAND runtime_test 7 0)
  set_tests_properties(runtime_active_miss PROPERTIES ENVIRONMENT MULATION_MUTANT=42)
endif()

if(MULATION_BUILD_EXAMPLES)
  add_subdirectory(examples/gtest-ctest)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ClangTools.cmake)
