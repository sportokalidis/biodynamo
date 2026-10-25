# Find one coherent ROOT installation. All metadata and tools must come from
# its root-config; mixing cached libraries or PATH tools breaks dictionaries.
include("${CMAKE_CURRENT_LIST_DIR}/ROOTCompatibility.cmake")
include(FindPackageHandleStandardArgs)
include(CMakeParseArguments)

find_program(ROOT_CONFIG_EXECUTABLE NAMES root-config
  HINTS "$ENV{ROOTSYS}/bin" "$ENV{BDM_ROOT_DIR}/bin" "${CMAKE_THIRD_PARTY_DIR}/root/bin")
if(NOT ROOT_CONFIG_EXECUTABLE)
  find_package_handle_standard_args(ROOT REQUIRED_VARS ROOT_CONFIG_EXECUTABLE)
  return()
endif()

bdm_root_query(--prefix ROOTSYS)
bdm_root_query(--version ROOT_VERSION)
bdm_root_query(--incdir ROOT_INCLUDE_DIR)
bdm_root_query(--etcdir ROOT_ETC_DIR)
bdm_root_query(--libdir ROOT_LIBRARY_DIR)
bdm_root_query(--cflags ROOT_CXX_FLAGS)
bdm_root_query(--ldflags ROOT_EXE_LINKER_FLAGS)
bdm_root_query(--features _root_features)
bdm_root_standard_from_metadata("${ROOT_CXX_FLAGS}" "${_root_features}" ROOT_CXX_STANDARD)
set(ROOT_INCLUDE_DIRS "${ROOT_INCLUDE_DIR}")
set(ROOT_ETC_DIRS "${ROOT_ETC_DIR}")
set(ROOT_LIBRARY_DIRS "${ROOT_LIBRARY_DIR}")
string(REGEX MATCHALL "-(D|U)[^ ]*" ROOT_DEFINITIONS "${ROOT_CXX_FLAGS}")
# C compiler invocations must not receive ROOT's C++ language flag.
string(REGEX REPLACE "-std=[^ ]+" "" ROOT_C_FLAGS "${ROOT_CXX_FLAGS}")
set(ROOT_USE_FILE "${CMAKE_CURRENT_LIST_DIR}/RootUseFile.cmake")

# A second discovery must not inherit language features from the old package.
foreach(_opt IN LISTS _root_options)
  unset(ROOT_${_opt}_FOUND)
endforeach()
foreach(_std 11 14 17 20 23 26)
  set(ROOT_cxx${_std}_FOUND FALSE)
endforeach()
separate_arguments(_root_options UNIX_COMMAND "${_root_features}")
foreach(_opt IN LISTS _root_options)
  set(ROOT_${_opt}_FOUND TRUE)
endforeach()

set(rootlibs Core RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics MathCore Thread MultiProc Imt)
list(APPEND rootlibs ${ROOT_FIND_COMPONENTS})
list(REMOVE_DUPLICATES rootlibs)
set(ROOT_LIBRARIES)
foreach(_cpt IN LISTS rootlibs)
  unset(ROOT_${_cpt}_LIBRARY CACHE)
  unset(ROOT_${_cpt}_LIBRARY)
  find_library(ROOT_${_cpt}_LIBRARY NAMES ${_cpt}
    PATHS "${ROOT_LIBRARY_DIR}" NO_DEFAULT_PATH)
  set(ROOT_${_cpt}_FOUND FALSE)
  if(ROOT_${_cpt}_LIBRARY)
    set(ROOT_${_cpt}_FOUND TRUE)
    list(APPEND ROOT_LIBRARIES "${ROOT_${_cpt}_LIBRARY}")
    mark_as_advanced(ROOT_${_cpt}_LIBRARY)
  endif()
endforeach()

foreach(_tool ROOTCLING GENREFLEX)
  string(TOLOWER "${_tool}" _name)
  unset(${_tool}_EXECUTABLE CACHE)
  unset(${_tool}_EXECUTABLE)
  find_program(${_tool}_EXECUTABLE NAMES ${_name}
    PATHS "${ROOTSYS}/bin" NO_DEFAULT_PATH)
endforeach()
find_package_handle_standard_args(ROOT
  REQUIRED_VARS ROOT_CONFIG_EXECUTABLE ROOTSYS ROOT_INCLUDE_DIR ROOT_LIBRARY_DIR
    ROOT_LIBRARIES ROOT_Core_LIBRARY ROOTCLING_EXECUTABLE GENREFLEX_EXECUTABLE
    ROOT_CXX_STANDARD
  VERSION_VAR ROOT_VERSION HANDLE_COMPONENTS)
mark_as_advanced(ROOT_CONFIG_EXECUTABLE ROOTCLING_EXECUTABLE GENREFLEX_EXECUTABLE)

# We use the launcher script to emulate a `source thisbdm.sh` call
if(NOT BDM_OUT_OF_SOURCE)
  set(LAUNCHER ${CMAKE_BINARY_DIR}/launcher.sh)
endif()

#----------------------------------------------------------------------------
# function ROOT_GENERATE_DICTIONARY( dictionary
#                                    header1 header2 ...
#                                    LINKDEF linkdef1 ...
#                                    OPTIONS opt1...)
function(ROOT_GENERATE_DICTIONARY dictionary)
  CMAKE_PARSE_ARGUMENTS(ARG "" "" "LINKDEF;OPTIONS" "" ${ARGN})
  #---Get the list of include directories------------------
  get_directory_property(incdirs INCLUDE_DIRECTORIES)
  set(includedirs)
  foreach( d ${incdirs})
     set(includedirs ${includedirs} -I${d})
  endforeach()
  #---Get the list of header files-------------------------
  set(headerfiles)
  foreach(fp ${ARG_UNPARSED_ARGUMENTS})
    if(${fp} MATCHES "[*?]") # Is this header a globbing expression?
      file(GLOB files ${fp})
      foreach(f ${files})
        if(NOT f MATCHES LinkDef) # skip LinkDefs from globbing result
          set(headerfiles ${headerfiles} ${f})
        endif()
      endforeach()
    else()
      find_file(headerFile ${fp} HINTS ${incdirs})
      set(headerfiles ${headerfiles} ${headerFile})
      unset(headerFile CACHE)
    endif()
  endforeach()
  #---Get LinkDef.h file------------------------------------
  set(linkdefs)
  foreach( f ${ARG_LINKDEF})
    find_file(linkFile ${f} HINTS ${incdirs})
    set(linkdefs ${linkdefs} ${linkFile})
    unset(linkFile CACHE)
  endforeach()
  #---call rootcling------------------------------------------
  # ROOT's dictionary tools use their built-in language mode. They do not
  # accept the compiler's -std= option (it is parsed as the unrelated -s).
  bdm_check_root_standard()
  add_custom_command(OUTPUT ${dictionary}.cxx
                     OUTPUT ${dictionary}_rdict.pcm
                     COMMAND ${LAUNCHER} ${ROOTCLING_EXECUTABLE} -f ${dictionary}.cxx
                                         ${ARG_OPTIONS} ${includedirs} ${headerfiles} ${linkdefs}
                     DEPENDS ${headerfiles} ${linkdefs} VERBATIM)
endfunction()

#----------------------------------------------------------------------------
# function REFLEX_GENERATE_DICTIONARY(dictionary
#                                     header1 header2 ...
#                                     SELECTION selectionfile ...
#                                     OPTIONS opt1...)
function(REFLEX_GENERATE_DICTIONARY dictionary)
  CMAKE_PARSE_ARGUMENTS(ARG "" "" "SELECTION;OPTIONS" "" ${ARGN})
  #---Get the list of header files-------------------------
  set(headerfiles)
  foreach(fp ${ARG_UNPARSED_ARGUMENTS})
    file(GLOB files ${fp})
    if(files)
      foreach(f ${files})
        set(headerfiles ${headerfiles} ${f})
      endforeach()
    else()
      set(headerfiles ${headerfiles} ${fp})
    endif()
  endforeach()
  #---Get Selection file------------------------------------
  if(IS_ABSOLUTE ${ARG_SELECTION})
    set(selectionfile ${ARG_SELECTION})
  else()
    set(selectionfile ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_SELECTION})
  endif()
  #---Get the list of include directories------------------
  get_directory_property(incdirs INCLUDE_DIRECTORIES)
  set(includedirs)
  foreach( d ${incdirs})
    set(includedirs ${includedirs} -I${d})
  endforeach()
  #---Get preprocessor definitions--------------------------
  get_directory_property(defs COMPILE_DEFINITIONS)
  foreach( d ${defs})
   set(definitions ${definitions} -D${d})
  endforeach()
  #---Nanes and others---------------------------------------
  set(gensrcdict ${dictionary}.cc)
  if(MSVC)
    set(gccxmlopts "--gccxmlopt=\"--gccxml-compiler cl\"")
  else()
    #set(gccxmlopts "--gccxmlopt=\'--gccxml-cxxflags -m64 \'")
    set(gccxmlopts)
  endif()
  #set(rootmapname ${dictionary}Dict.rootmap)
  #set(rootmapopts --rootmap=${rootmapname} --rootmap-lib=${libprefix}${dictionary}Dict)
  #---Actual command----------------------------------------
  bdm_check_root_standard()
  add_custom_command(OUTPUT ${gensrcdict} ${rootmapname} ${dictionary}_rdict.pcm
                     COMMAND ${LAUNCHER} ${GENREFLEX_EXECUTABLE} ${headerfiles} -o ${gensrcdict} ${rootmapopts} --select=${selectionfile} --noIncludePaths
                            ${ARG_OPTIONS} ${includedirs} ${definitions}
                     DEPENDS ${headerfiles} ${selectionfile})
endfunction()
