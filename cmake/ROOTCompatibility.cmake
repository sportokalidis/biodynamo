include_guard(GLOBAL)

# The final C++23 value of __cplusplus. Early implementations such as GCC 13
# report the provisional 202100L instead; ROOT's clang-based Cling always
# reports the final value.
set(BDM_CXX23_CPLUSPLUS 202302)

# Query the selected installation, never a second root-config from PATH.
function(bdm_root_query option output)
  execute_process(COMMAND "${ROOT_CONFIG_EXECUTABLE}" "${option}"
    RESULT_VARIABLE result OUTPUT_VARIABLE value ERROR_VARIABLE error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "${ROOT_CONFIG_EXECUTABLE} ${option} failed "
      "(${result}): ${error}. Select a working ROOT installation.")
  endif()
  set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(bdm_root_standard_from_metadata flags features output)
  set(standards)
  separate_arguments(flag_list UNIX_COMMAND "${flags}")
  foreach(flag IN LISTS flag_list)
    if(flag MATCHES "^-std=(c|gnu)\\+\\+(.+)$")
      set(standard "${CMAKE_MATCH_2}")
      if(standard STREQUAL "1z")
        set(standard 17)
      elseif(standard STREQUAL "2a")
        set(standard 20)
      elseif(standard STREQUAL "2b")
        set(standard 23)
      elseif(standard STREQUAL "2c")
        set(standard 26)
      endif()
      if(NOT standard MATCHES "^(11|14|17|20|23|26)$")
        message(FATAL_ERROR "Unknown ROOT C++ standard flag: ${flag}")
      endif()
      list(APPEND standards "${standard}")
    endif()
  endforeach()
  separate_arguments(feature_list UNIX_COMMAND "${features}")
  foreach(feature IN LISTS feature_list)
    if(feature MATCHES "^cxx([0-9]+)$")
      list(APPEND standards "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES standards)
  list(LENGTH standards count)
  if(NOT count EQUAL 1)
    message(FATAL_ERROR "Cannot determine a single ROOT C++ standard from "
      "--cflags='${flags}' and --features='${features}' (found '${standards}').")
  endif()
  set(${output} "${standards}" PARENT_SCOPE)
endfunction()

# ROOT bakes its build compiler's __cplusplus into the public RConfigure.h and
# warns from that header whenever a consumer disagrees. The same value also
# selects preprocessor branches in ROOT's headers, so a consumer which differs
# can compile against declarations ROOT's own libraries were not built with.
# Returns an empty value when the macro is absent.
function(bdm_root_cplusplus_from_header incdir output)
  set(header "${incdir}/RConfigure.h")
  if(NOT EXISTS "${header}")
    message(FATAL_ERROR "'${header}' is missing. '${ROOT_CONFIG_EXECUTABLE} "
      "--incdir' does not point at a complete ROOT installation.")
  endif()
  file(STRINGS "${header}" matches
    REGEX "^[ \t]*#[ \t]*define[ \t]+ROOT__cplusplus[ \t]+[0-9]+L?[ \t]*$")
  set(values)
  foreach(line IN LISTS matches)
    string(REGEX REPLACE "^.*ROOT__cplusplus[ \t]+([0-9]+)L?[ \t]*$" "\\1"
      value "${line}")
    list(APPEND values "${value}")
  endforeach()
  list(REMOVE_DUPLICATES values)
  list(LENGTH values count)
  if(count GREATER 1)
    message(FATAL_ERROR "'${header}' defines ROOT__cplusplus more than once "
      "(${values}). Reinstall ROOT from a clean build directory.")
  endif()
  set(${output} "${values}" PARENT_SCOPE)
endfunction()

# Preprocess __cplusplus with the selected compiler and standard. A compile
# feature only states that C++23 is accepted, not which value it reports.
function(bdm_compiler_cplusplus output)
  set(probe "${CMAKE_BINARY_DIR}/CMakeFiles/bdm-cplusplus-probe.cc")
  file(WRITE "${probe}" "BDM__cplusplus __cplusplus\n")
  if(NOT DEFINED BDM_CXX_STANDARD)
    message(FATAL_ERROR "Apply the BioDynaMo language policy before probing __cplusplus.")
  endif()
  set(standard_option "${CMAKE_CXX${BDM_CXX_STANDARD}_STANDARD_COMPILE_OPTION}")
  execute_process(COMMAND "${CMAKE_CXX_COMPILER}"
      ${standard_option} -E -P "${probe}"
    RESULT_VARIABLE result OUTPUT_VARIABLE value ERROR_VARIABLE error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${result}" STREQUAL "0" OR
     NOT value MATCHES "BDM__cplusplus[ \t]+([0-9]+)L?")
    message(FATAL_ERROR "Cannot determine __cplusplus for ${CMAKE_CXX_COMPILER} "
      "${standard_option} (${result}): ${error}${value}")
  endif()
  set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(bdm_check_cplusplus_agreement root_value compiler_value)
  if(NOT root_value)
    message(WARNING "ROOT ${ROOT_VERSION} at '${ROOTSYS}' does not define "
      "ROOT__cplusplus, so its build compiler's C++23 value cannot be "
      "compared with ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
      "(__cplusplus=${compiler_value}).")
    return()
  endif()
  if(NOT "${root_value}" STREQUAL "${compiler_value}")
    message(FATAL_ERROR "ROOT ${ROOT_VERSION} at '${ROOTSYS}' was built by a "
      "compiler reporting __cplusplus=${root_value}, but "
      "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} reports "
      "${compiler_value}. Both select C++23, yet ROOT's public RConfigure.h "
      "warns on every translation unit and its headers can expose different "
      "declarations than its libraries were built with. Use a ROOT package "
      "built by this compiler, or select a compiler reporting ${root_value} "
      "with CC/CXX, and configure in a fresh build directory.")
  endif()
  if("${BDM_CXX_STANDARD}" STREQUAL "23" AND
     NOT "${root_value}" STREQUAL "${BDM_CXX23_CPLUSPLUS}")
    message(WARNING "ROOT and ${CMAKE_CXX_COMPILER_ID} "
      "${CMAKE_CXX_COMPILER_VERSION} agree on the provisional C++23 value "
      "__cplusplus=${root_value}, but Cling reports "
      "${BDM_CXX23_CPLUSPLUS}, so ROOT's headers warn whenever they are "
      "interpreted, including during dictionary generation. For a combination "
      "without that warning use GCC 14 or newer, or Clang 17 or newer, "
      "together with a ROOT package built by the same compiler.")
  endif()
endfunction()

function(bdm_check_root_cplusplus_macro)
  # Ask the selected installation rather than reading a discovery variable,
  # which the download path sets in a different scope.
  bdm_root_query(--incdir incdir)
  bdm_root_cplusplus_from_header("${incdir}" root_value)
  bdm_compiler_cplusplus(compiler_value)
  bdm_check_cplusplus_agreement("${root_value}" "${compiler_value}")
  message(STATUS "ROOT and compiler __cplusplus: ${compiler_value}")
endfunction()

# The main build has no standard of its own yet and adopts ROOT's. An installed
# consumer, or a build whose standard was requested explicitly, already has one
# and must match the package exactly: Cling parses the same headers the
# libraries were compiled from.
function(bdm_check_root_standard)
  if(DEFINED BDM_CXX_STANDARD AND
     NOT "${ROOT_CXX_STANDARD}" STREQUAL "${BDM_CXX_STANDARD}")
    message(FATAL_ERROR "ROOT ${ROOT_VERSION} at '${ROOTSYS}' uses C++"
      "${ROOT_CXX_STANDARD}, but this build requires C++${BDM_CXX_STANDARD}. "
      "Select a ROOT package built with C++${BDM_CXX_STANDARD}, source its "
      "bin/thisroot.sh, and configure in a fresh build directory. Do not reuse "
      "dictionaries or libraries built in a different language mode.")
  endif()
  message(STATUS "ROOT ${ROOT_VERSION}: C++${ROOT_CXX_STANDARD} at ${ROOTSYS}")
endfunction()
