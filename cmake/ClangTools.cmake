# clang-format and clang-tidy (optional locally, required in CI)

find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format)
find_program(CLANG_TIDY_EXE NAMES clang-tidy-18 clang-tidy)

set(_mulation_format_files)
foreach(_dir runner runtime include tests examples plugin)
  file(GLOB_RECURSE _part
    "${CMAKE_SOURCE_DIR}/${_dir}/*.c"
    "${CMAKE_SOURCE_DIR}/${_dir}/*.h"
    "${CMAKE_SOURCE_DIR}/${_dir}/*.cpp"
    "${CMAKE_SOURCE_DIR}/${_dir}/*.hpp")
  list(APPEND _mulation_format_files ${_part})
endforeach()

if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${_mulation_format_files}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "clang-format -i")
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run -Werror ${_mulation_format_files}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "clang-format --dry-run -Werror")
else()
  add_custom_target(format
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-clang-format.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_custom_target(format-check
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-clang-format.sh --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

add_custom_target(tidy
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-clang-tidy.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "clang-tidy")

option(MULATION_ENABLE_CLANG_TIDY "Run clang-tidy during compilation (not the LLVM plugin)" OFF)
if(MULATION_ENABLE_CLANG_TIDY)
  if(NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR "MULATION_ENABLE_CLANG_TIDY=ON but clang-tidy-18 was not found")
  endif()
  set(_tidy_cmd ${CLANG_TIDY_EXE};--quiet)
  foreach(_tgt mulation-run mulation mulation_runtime mulation_unit_tests runtime_test)
    if(TARGET ${_tgt})
      set_target_properties(${_tgt} PROPERTIES
        C_CLANG_TIDY "${_tidy_cmd}"
        CXX_CLANG_TIDY "${_tidy_cmd}")
    endif()
  endforeach()
endif()
