# BioDynaMo, its public headers, and ROOT's interpreter must agree on the
# language mode. That mode is a property of the selected ROOT package rather
# than a fixed value: ROOT supports C++17, 20 and 23 from one source tree and
# its published packages differ per platform and release. The main build
# therefore derives the standard after ROOT discovery, while an installed
# consumer applies the standard BioDynaMo was built with.
include_guard(GLOBAL)

# Modes this project builds and tests. ROOT also accepts 20, but BioDynaMo has
# no coverage for it, so it is rejected rather than silently accepted.
set(BDM_SUPPORTED_CXX_STANDARDS 17 23)

if(CMAKE_VERSION VERSION_LESS 3.24)
  message(FATAL_ERROR "BioDynaMo requires CMake 3.24 or newer.")
endif()

# Explicit flags can override CMake's standard in custom commands and exported
# bdm-config output. Require one source of truth instead of ordering -std flags.
foreach(_bdm_flags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL
    CMAKE_CXX_FLAGS_OPTIMIZED CMAKE_CXX_FLAGS_DEBUGFULL CMAKE_CXX_FLAGS_COVERAGE)
  if("${${_bdm_flags}}" MATCHES "(^|[ ;])(-std=|/std:)")
    message(FATAL_ERROR "Remove the explicit standard flag from ${_bdm_flags}; "
      "BioDynaMo derives CMAKE_CXX_STANDARD from the selected ROOT package for "
      "both targets and consumers.")
  endif()
endforeach()

# A macro so the standard lands in the calling scope; call it from directory
# scope once the standard is known.
macro(bdm_apply_cxx_standard _bdm_standard)
  if(NOT "${_bdm_standard}" IN_LIST BDM_SUPPORTED_CXX_STANDARDS)
    message(FATAL_ERROR "BioDynaMo does not support C++${_bdm_standard}. "
      "Supported standards are: ${BDM_SUPPORTED_CXX_STANDARDS}. Select a ROOT "
      "package built with one of them.")
  endif()
  # A consumer pinning a different standard would disagree with the installed
  # libraries and with ROOT's interpreter.
  if(DEFINED CMAKE_CXX_STANDARD AND
     NOT "${CMAKE_CXX_STANDARD}" STREQUAL "${_bdm_standard}")
    message(FATAL_ERROR "This build requires C++${_bdm_standard} to match its "
      "ROOT package, but CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}. Remove the "
      "conflicting setting and configure in a fresh build directory.")
  endif()
  if(NOT "cxx_std_${_bdm_standard}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
      "(${CMAKE_CXX_COMPILER}) does not support the C++${_bdm_standard} mode "
      "this ROOT package requires. Select a newer compiler with CC/CXX in a "
      "fresh build directory.")
  endif()
  set(BDM_CXX_STANDARD "${_bdm_standard}")
  # The spelling this compiler wants, e.g. -std=c++23 or -std=c++2b. Custom
  # commands, bdm-config and external builds must use the same one.
  set(BDM_CXX_STANDARD_COMPILE_OPTION
    "${CMAKE_CXX${_bdm_standard}_STANDARD_COMPILE_OPTION}")
  set(CMAKE_CXX_STANDARD "${_bdm_standard}")
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  message(STATUS "BioDynaMo C++ standard: ${BDM_CXX_STANDARD} (extensions disabled)")
endmacro()

# Installed consumers know the standard BioDynaMo was built with up front; the
# main build leaves it unset here and derives it from ROOT instead.
if(DEFINED BioDynaMo_CXX_STANDARD AND NOT "${BioDynaMo_CXX_STANDARD}" STREQUAL "")
  bdm_apply_cxx_standard(${BioDynaMo_CXX_STANDARD})
endif()
