include(utils)

SET(PARAVIEW_SOURCE_DIR "${CMAKE_THIRD_PARTY_DIR}/paraview")

if(APPLE AND "${DETECTED_ARCH}" STREQUAL "i386")
  # The release of cmake 3.23.0 broke our build of ParaView on MacOSX. 
  # The build was fixed with a reupload and carries the additional tag cm233.
  SET(PARAVIEW_TAR_FILE paraview_v5.10.0_cm323_${DETECTED_OS_VERS}_default.tar.gz)
elseif(APPLE AND "${DETECTED_ARCH}" STREQUAL "arm64")
  SET(PARAVIEW_TAR_FILE paraview_v5.10.0_${DETECTED_OS_VERS}_default.tar.gz)
else()
  SET(PARAVIEW_TAR_FILE paraview_v5.9.0_${DETECTED_OS_VERS}_default.tar.gz)
  if(${DETECTED_OS_VERS} STREQUAL ubuntu-24.04)
    SET(PARAVIEW_TAR_FILE paraview_v5.9.0_ubuntu-22.04_default.tar.gz)
  endif()
endif()
set(PARAVIEW_SHA_KEY ${DETECTED_OS_VERS}-ParaView)
set(PARAVIEW_SHA ${${PARAVIEW_SHA_KEY}})

# message(STATUS "Using  ParaView tarball    : ${PARAVIEW_TAR_FILE}")
# message(STATUS "Using  ParaView source dir : ${PARAVIEW_SOURCE_DIR}")
# message(STATUS "Using  ParaView SHA key    : ${PARAVIEW_SHA_KEY}")
# message(STATUS "Verify ParaView SHA        : ${PARAVIEW_SHA}")


if(${DETECTED_OS_VERS} STREQUAL ubuntu-24.04)
  # # Define the URL and destination
  # set(PARAVIEW_TARBALL_URL "https://cernbox.cern.ch/s/EEi5Jeu4e9bn0nr/download")
  # set(PARAVIEW_TARBALL "${PARAVIEW_SOURCE_DIR}/paraview-5.13.3.tar.gz")

  # # Download the tarball
  # file(DOWNLOAD
  #   ${PARAVIEW_TARBALL_URL}
  #   ${PARAVIEW_TARBALL}
  #   SHOW_PROGRESS
  #   STATUS download_status
  # )

  # # Check if download succeeded (optional, but safer)
  # list(GET download_status 0 status_code)
  # if(NOT status_code EQUAL 0)
  #   message(FATAL_ERROR "Failed to download ParaView tarball: ${download_status}")
  # endif()

  # # Extract the tarball
  # execute_process(
  #   COMMAND ${CMAKE_COMMAND} -E tar xzf ${PARAVIEW_TARBALL}
  #   WORKING_DIRECTORY ${PARAVIEW_SOURCE_DIR}
  # )

  message(STATUS "Using  ParaView tarball    : paraview-5.13.3.tar.gz")
  message(STATUS "Using  ParaView source dir : ${PARAVIEW_SOURCE_DIR}")
  message(STATUS "Using  ParaView SHA key    : ${PARAVIEW_SHA_KEY}")
  message(STATUS "Verify ParaView SHA        : ${PARAVIEW_SHA}")

  # download_verify_extract(
  #   https://cernbox.cern.ch/s/EEi5Jeu4e9bn0nr/download
  #   ${PARAVIEW_SOURCE_DIR}
  #   ${PARAVIEW_SHA}
  # )

  # set(PARAVIEW_SOURCE_DIR "${CMAKE_BINARY_DIR}/third_party/paraview")
  set(PARAVIEW_TARBALL "${PARAVIEW_SOURCE_DIR}/paraview-5.13.3.tar.gz")

  # Make sure the destination exists
  file(MAKE_DIRECTORY ${PARAVIEW_SOURCE_DIR})

  # Set the OneDrive direct download URL (ensure this actually works)
  set(PARAVIEW_URL "https://www.dropbox.com/scl/fi/3c5gfrj13y75v9lj5t43z/paraview-5.13.3.tar.gz?rlkey=dqpjmnqvqa7jkhn1omdrni6ic&st=euxrl5ul&dl=1")

  # Download the file
  file(DOWNLOAD
    "${PARAVIEW_URL}"
    "${PARAVIEW_TARBALL}"
    SHOW_PROGRESS
    STATUS download_status
    INACTIVITY_TIMEOUT 60
  )

  # Check for download success
  list(GET download_status 0 status_code)
  if(NOT status_code EQUAL 0)
    message(FATAL_ERROR "Download failed: ${download_status}")
  endif()

  # Extract the tarball
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf "${PARAVIEW_TARBALL}"
    WORKING_DIRECTORY "${PARAVIEW_SOURCE_DIR}"
  )
else()

  message(STATUS "Using  ParaView tarball    : ${PARAVIEW_TAR_FILE}")
  message(STATUS "Using  ParaView source dir : ${PARAVIEW_SOURCE_DIR}")
  message(STATUS "Using  ParaView SHA key    : ${PARAVIEW_SHA_KEY}")
  message(STATUS "Verify ParaView SHA        : ${PARAVIEW_SHA}")

  download_verify_extract(
    http://cern.ch/biodynamo-lfs/third-party/${PARAVIEW_TAR_FILE}
    ${PARAVIEW_SOURCE_DIR}
    ${PARAVIEW_SHA}
  )
endif()


