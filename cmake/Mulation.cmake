# Mulation CMake helpers (compat layer — does not replace gtest/doctest/ZTEST/CTest)
#
#   include(Mulation)   # or list(APPEND CMAKE_MODULE_PATH ...)
#   mulation_instrument(my_lib)                    # SUT only, never test sources
#   mulation_instrument_sources(app SOURCES a.c)   # per-source (Zephyr-style app targets)
#   mulation_add_check(my_tests [ARGS ...] ...)    # mulation-check-<target> + umbrella mulation-check
#
# Your unit runner stays the unit-test runner. mulation-check is the pre-production gate.
# Optional at compile time: MULATION_SKIP_PATHS=colon:separated:substrings

if(NOT DEFINED MULATION_PLUGIN_PATH)
  if(TARGET mulation_plugin)
    set(MULATION_PLUGIN_PATH "$<TARGET_FILE:mulation_plugin>")
  else()
    # Installed at <libdir>/cmake/Mulation/Mulation.cmake; plugin lives in <libdir>.
    get_filename_component(_mulation_libdir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    set(MULATION_PLUGIN_PATH "${_mulation_libdir}/libmulation_plugin.so")
  endif()
endif()

if(NOT DEFINED MULATION_RUNTIME_TARGET)
  if(TARGET mulation_runtime)
    set(MULATION_RUNTIME_TARGET mulation_runtime)
  else()
    get_filename_component(_mulation_libdir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    set(_mulation_incdir "")
    foreach(_cand
        "${CMAKE_CURRENT_LIST_DIR}/../../../include"
        "${CMAKE_CURRENT_LIST_DIR}/../../../../include"
        "${_mulation_libdir}/../include")
      if(EXISTS "${_cand}/mulation/mulation.h")
        get_filename_component(_mulation_incdir "${_cand}" ABSOLUTE)
        break()
      endif()
    endforeach()
    if(NOT TARGET mulation::runtime)
      add_library(mulation::runtime STATIC IMPORTED GLOBAL)
      set_target_properties(mulation::runtime PROPERTIES
        IMPORTED_LOCATION "${_mulation_libdir}/libmulation_runtime.a")
      if(_mulation_incdir)
        set_target_properties(mulation::runtime PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${_mulation_incdir}")
      endif()
    endif()
    set(MULATION_RUNTIME_TARGET mulation::runtime)
  endif()
endif()

if(NOT DEFINED MULATION_RUNNER)
  if(TARGET mulation-run)
    set(MULATION_RUNNER "$<TARGET_FILE:mulation-run>")
  elseif(TARGET mulation)
    set(MULATION_RUNNER "$<TARGET_FILE:mulation>")
  else()
    set(MULATION_RUNNER mulation-run)
  endif()
endif()

set(MULATION_MIN_SCORE "0" CACHE STRING "CI gate: minimum mutation score percent")
option(MULATION_GIT_DIFF "Only mutants on git-diff lines" OFF)

function(mulation_instrument target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "mulation_instrument: target '${target}' does not exist")
  endif()
  target_compile_options(${target} PRIVATE
    -g
    "-fpass-plugin=${MULATION_PLUGIN_PATH}"
  )
  if(TARGET mulation_plugin)
    add_dependencies(${target} mulation_plugin)
  endif()
  if(TARGET ${MULATION_RUNTIME_TARGET})
    target_link_libraries(${target} PRIVATE ${MULATION_RUNTIME_TARGET})
    target_include_directories(${target} PRIVATE
      $<TARGET_PROPERTY:${MULATION_RUNTIME_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
  endif()
endfunction()

# Instrument selected sources only (Zephyr `app` / single-binary targets).
#   mulation_instrument_sources(app SOURCES src/foo.c src/bar.c)
function(mulation_instrument_sources target)
  cmake_parse_arguments(MIS "" "" "SOURCES" ${ARGN})
  if(NOT TARGET ${target})
    message(FATAL_ERROR "mulation_instrument_sources: target '${target}' does not exist")
  endif()
  if(NOT MIS_SOURCES)
    message(FATAL_ERROR "mulation_instrument_sources: SOURCES is required")
  endif()
  foreach(_src IN LISTS MIS_SOURCES)
    set_source_files_properties(${_src} PROPERTIES
      COMPILE_OPTIONS "-g;-fpass-plugin=${MULATION_PLUGIN_PATH}")
  endforeach()
  if(TARGET mulation_plugin)
    add_dependencies(${target} mulation_plugin)
  endif()
  if(TARGET ${MULATION_RUNTIME_TARGET})
    target_link_libraries(${target} PRIVATE ${MULATION_RUNTIME_TARGET})
    target_include_directories(${target} PRIVATE
      $<TARGET_PROPERTY:${MULATION_RUNTIME_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
  endif()
endfunction()

# Register a mutation gate. Creates mulation-check-<test_target> and wires it into
# an umbrella `mulation-check` target (created on first call).
#
#   mulation_add_check(my_tests
#     [MIN_SCORE N] [JOBS N] [TEST_FILTER pat] [FRAMEWORK name]
#     [ARGS arg1 arg2 ...]          # appended after the test binary
#     [BINARIES path1 path2 ...]    # extra --binary
#     [COMMAND cmd [args...]]       # override command (default: $<TARGET_FILE:test_target>)
#   )
function(mulation_add_check test_target)
  cmake_parse_arguments(MAC "" "MIN_SCORE;JOBS;TEST_FILTER;FRAMEWORK" "ARGS;BINARIES;COMMAND" ${ARGN})
  if(NOT MAC_MIN_SCORE)
    set(MAC_MIN_SCORE ${MULATION_MIN_SCORE})
  endif()
  if(NOT TARGET ${test_target})
    message(FATAL_ERROR "mulation_add_check: target '${test_target}' does not exist")
  endif()

  set(_extra)
  if(MULATION_GIT_DIFF)
    list(APPEND _extra --git-diff)
  endif()
  if(MAC_JOBS)
    list(APPEND _extra -j ${MAC_JOBS})
  endif()
  if(MAC_TEST_FILTER)
    list(APPEND _extra --test-filter ${MAC_TEST_FILTER})
  endif()
  if(MAC_FRAMEWORK)
    list(APPEND _extra --framework ${MAC_FRAMEWORK})
  endif()
  foreach(_b IN LISTS MAC_BINARIES)
    list(APPEND _extra --binary ${_b})
  endforeach()

  if(MAC_COMMAND)
    set(_cmd ${MAC_COMMAND})
  else()
    set(_cmd "$<TARGET_FILE:${test_target}>")
  endif()

  set(_check_target "mulation-check-${test_target}")
  add_custom_target(${_check_target}
    COMMAND ${MULATION_RUNNER}
            --min-score ${MAC_MIN_SCORE}
            ${_extra}
            -- ${_cmd} ${MAC_ARGS}
    DEPENDS ${test_target}
    USES_TERMINAL
    COMMENT "Mutation campaign for ${test_target}"
  )
  if(TARGET mulation-run)
    add_dependencies(${_check_target} mulation-run)
  elseif(TARGET mulation)
    add_dependencies(${_check_target} mulation)
  endif()

  if(NOT TARGET mulation-check)
    add_custom_target(mulation-check
      COMMENT "Mutation campaign (all mulation-check-* targets)")
  endif()
  add_dependencies(mulation-check ${_check_target})
endfunction()
