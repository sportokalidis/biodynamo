include(utils)

SET(PARAVIEW_SOURCE_DIR "${CMAKE_THIRD_PARTY_DIR}/paraview")

if(APPLE AND "${DETECTED_ARCH}" STREQUAL "i386")
  # The release of cmake 3.23.0 broke our build of ParaView on MacOSX. 
  # The build was fixed with a reupload and carries the additional tag cm233.
  SET(PARAVIEW_TAR_FILE paraview_v5.10.0_cm323_${DETECTED_OS_VERS}_default.tar.gz)
elseif(APPLE AND "${DETECTED_ARCH}" STREQUAL "arm64")
  SET(PARAVIEW_TAR_FILE paraview_v5.10.0_${DETECTED_OS_VERS}_default.tar.gz)
else()
  SET(PARAVIEW_TAR_FILE paraview_v5.13.3_${DETECTED_OS_VERS}_default.tar.gz)
  if(${DETECTED_OS_VERS} STREQUAL ubuntu-24.04)
    SET(PARAVIEW_TAR_FILE paraview_v5.13.3_ubuntu-22.04_default.tar.gz)
  endif()
endif()
set(PARAVIEW_SHA_KEY ${DETECTED_OS_VERS}-ParaView)
set(PARAVIEW_SHA ${${PARAVIEW_SHA_KEY}})

message(STATUS "Using  ParaView tarball    : ${PARAVIEW_TAR_FILE}")
message(STATUS "Using  ParaView source dir : ${PARAVIEW_SOURCE_DIR}")
message(STATUS "Using  ParaView SHA key    : ${PARAVIEW_SHA_KEY}")
message(STATUS "Verify ParaView SHA        : ${PARAVIEW_SHA}")


if(${DETECTED_OS_VERS} STREQUAL ubuntu-22.04 OR ${DETECTED_OS_VERS} STREQUAL ubuntu-24.04)
  # Define the URL and destination
  set(PARAVIEW_TARBALL_URL "https://cernbox.cern.ch/s/EEi5Jeu4e9bn0nr/download")
  set(PARAVIEW_TARBALL "${PARAVIEW_SOURCE_DIR}/paraview-5.13.3.tar.gz")

  # Download the tarball
  file(DOWNLOAD
    ${PARAVIEW_TARBALL_URL}
    ${PARAVIEW_TARBALL}
    SHOW_PROGRESS
    STATUS download_status
  )

  # Check if download succeeded (optional, but safer)
  list(GET download_status 0 status_code)
  if(NOT status_code EQUAL 0)
    message(FATAL_ERROR "Failed to download ParaView tarball: ${download_status}")
  endif()

  # Extract the tarball
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${PARAVIEW_TARBALL}
    WORKING_DIRECTORY ${PARAVIEW_SOURCE_DIR}
  )
  
else()
  download_verify_extract(
    http://cern.ch/biodynamo-lfs/third-party/${PARAVIEW_TAR_FILE}
    ${PARAVIEW_SOURCE_DIR}
    ${PARAVIEW_SHA}
  )
endif()


