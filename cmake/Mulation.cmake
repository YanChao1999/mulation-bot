# Mulation CMake helpers (compat layer — does not replace Google Test or CTest)
#
#   include(Mulation)   # or list(APPEND CMAKE_MODULE_PATH ...)
#   mulation_instrument(my_lib)           # SUT only, never test sources
#   mulation_add_check(my_tests)          # custom target mulation-check
#
# ctest stays the unit-test runner. mulation-check is the pre-production gate.

if(NOT DEFINED MULATION_PLUGIN_PATH)
  if(TARGET mulation_plugin)
    set(MULATION_PLUGIN_PATH "$<TARGET_FILE:mulation_plugin>")
  else()
    get_filename_component(_mulation_cmake_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
    set(MULATION_PLUGIN_PATH "${_mulation_cmake_dir}/../lib/libmulation_plugin.so")
  endif()
endif()

if(NOT DEFINED MULATION_RUNTIME_TARGET)
  if(TARGET mulation_runtime)
    set(MULATION_RUNTIME_TARGET mulation_runtime)
  else()
    set(MULATION_RUNTIME_TARGET mulation_runtime)
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

function(mulation_add_check test_target)
  cmake_parse_arguments(MAC "" "MIN_SCORE" "" ${ARGN})
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

  add_custom_target(mulation-check
    COMMAND ${MULATION_RUNNER}
            --min-score ${MAC_MIN_SCORE}
            ${_extra}
            -- $<TARGET_FILE:${test_target}>
    DEPENDS ${test_target}
    USES_TERMINAL
    COMMENT "Mutation campaign (prove tests would catch bugs before production)"
  )
  if(TARGET mulation-run)
    add_dependencies(mulation-check mulation-run)
  elseif(TARGET mulation)
    add_dependencies(mulation-check mulation)
  endif()
endfunction()
