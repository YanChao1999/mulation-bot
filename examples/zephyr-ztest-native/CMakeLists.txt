# Zephyr out-of-tree sample: ZTEST on native_sim + mulation instrumentation.
#
#   west build -b native_sim -d build-native path/to/examples/zephyr-ztest-native
#   ./build-native/zephyr/zephyr.exe
#   mulation --git-diff -j 4 -- ./build-native/zephyr/zephyr.exe
#
# Requires ZEPHYR_BASE and an LLVM/Clang Zephyr toolchain (pass plugin).

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mulation_zephyr_ztest_native)

target_sources(app PRIVATE
  src/compare.c
  src/compare_test.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- mulation: instrument SUT only (not ZTEST / Zephyr) ---
set(_mulation_prefix "$ENV{HOME}/.local")
if(DEFINED ENV{MULATION_PREFIX} AND NOT "$ENV{MULATION_PREFIX}" STREQUAL "")
  set(_mulation_prefix "$ENV{MULATION_PREFIX}")
endif()
set(_mulation_plugin "${_mulation_prefix}/lib/libmulation_plugin.so")
set(_mulation_runtime "${_mulation_prefix}/lib/libmulation_runtime.a")
set(_mulation_inc "${_mulation_prefix}/include")

if(EXISTS "${_mulation_plugin}" AND EXISTS "${_mulation_runtime}")
  message(STATUS "mulation: instrumenting src/compare.c via ${_mulation_plugin}")
  set_source_files_properties(src/compare.c PROPERTIES
    COMPILE_OPTIONS "-g;-fpass-plugin=${_mulation_plugin}"
  )
  target_include_directories(app PRIVATE ${_mulation_inc})
  target_link_libraries(app PRIVATE ${_mulation_runtime})
else()
  message(WARNING
    "mulation plugin/runtime not found under ${_mulation_prefix}/lib — "
    "building without instrumentation. Install mulation or set MULATION_PREFIX.")
endif()
