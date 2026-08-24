cmake_minimum_required(VERSION 3.16)
project(mulation-bot VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(WARNING "Build the pass plugin with Clang: cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

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

set(_runner_src
  runner/main.cpp
  runner/catalog.cpp
  runner/process.cpp
  runner/diff.cpp
)
add_executable(mulation-run ${_runner_src})
target_include_directories(mulation-run PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runner)
add_executable(mulation ${_runner_src})
target_include_directories(mulation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runner)

include(GNUInstallDirs)
install(TARGETS mulation_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS mulation_runtime ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS mulation-run mulation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/mulation DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES cmake/Mulation.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mulation)

set(MULATION_PLUGIN_PATH "$<TARGET_FILE:mulation_plugin>" CACHE INTERNAL "")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Mulation.cmake)

option(MULATION_BUILD_EXAMPLES "Build the gtest/CTest example" ON)
if(MULATION_BUILD_EXAMPLES)
  enable_testing()
  add_subdirectory(examples/gtest-ctest)
endif()
