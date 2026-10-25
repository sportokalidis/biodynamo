include(utils)

# Directory in which ROOT will be downloaded
SET(ROOT_SOURCE_DIR "${CMAKE_THIRD_PARTY_DIR}/root")

# Shared with verify_ROOT() so the download and cache check agree.
bdm_root_download(ROOT_URL ROOT_SHA)

message(STATUS "Using  ROOT archive    : ${ROOT_URL}")
message(STATUS "Using  ROOT source dir : ${ROOT_SOURCE_DIR}")
message(STATUS "Verify ROOT SHA        : ${ROOT_SHA}")

download_verify_extract(
  ${ROOT_URL}
  ${ROOT_SOURCE_DIR}
  ${ROOT_SHA}
)

# CMakeLists.txt repairs ROOT library paths after verify_ROOT(), including cached installs.

# Run again find_package in order to find ROOT
# Do not rediscover another installation from a previously sourced environment.
set(ROOT_CONFIG_EXECUTABLE "${ROOT_SOURCE_DIR}/bin/root-config" CACHE FILEPATH
  "ROOT configuration executable" FORCE)
find_package(ROOT COMPONENTS Geom Gui GenVector REQUIRED)

# Set ROOTSYS variable
string(REGEX REPLACE "/include$" "" TMP_ROOT_PATH "${ROOT_INCLUDE_DIRS}")
set(ENV{ROOTSYS} ${TMP_ROOT_PATH})

# Set ROOT_CONFIG_EXECUTABLE variable
find_program(ROOT_CONFIG_EXECUTABLE NAMES root-config HINTS "${TMP_ROOT_PATH}/bin")
SET(ROOT_CONFIG_EXECUTABLE ${ROOT_CONFIG_EXECUTABLE} PARENT_SCOPE)
