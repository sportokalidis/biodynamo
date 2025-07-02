include(utils)

SET(QT_SOURCE_DIR "${CMAKE_THIRD_PARTY_DIR}/qt")

set(QT_TAR_FILE qt_v5.12.10_${DETECTED_OS_VERS}.tar.gz)

if(${DETECTED_OS_VERS} STREQUAL ubuntu-24.04)
    set(QT_TAR_FILE qt_v5.12.10_ubuntu-22.04.tar.gz)
endif()


if(${DETECTED_OS_VERS} STREQUAL ubuntu-24.04 OR ${DETECTED_OS_VERS} STREQUAL ubuntu-24.04)
  # Define the URL and destination
  set(QT_TARBALL_URL "https://cernbox.cern.ch/s/L5E6F5w0U2K3GdF/download")
  set(QT_TARBALL "${QT_SOURCE_DIR}/qt-5.15.2.tar.gz")

  # Download the tarball
  file(DOWNLOAD
    ${QT_TARBALL_URL}
    ${QT_TARBALL}
    SHOW_PROGRESS
    STATUS qt_download_status
  )

  # Check if download succeeded (optional)
  list(GET qt_download_status 0 qt_status_code)
  if(NOT qt_status_code EQUAL 0)
    message(FATAL_ERROR "Failed to download Qt tarball: ${qt_download_status}")
  endif()

  # Extract the tarball
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${QT_TARBALL}
    WORKING_DIRECTORY ${QT_SOURCE_DIR}
  )
else()
  download_verify_extract(
    http://cern.ch/biodynamo-lfs/third-party/${QT_TAR_FILE}
    ${QT_SOURCE_DIR}
    ${${DETECTED_OS_VERS}-Qt}
  )
endif()

# temporal workaround to avoid libprotobuf error for paraview
# use only until patched archive has been uploaded
IF (NOT APPLE)
    execute_process(COMMAND rm ${QT_SOURCE_DIR}/plugins/platformthemes/libqgtk3.so
            WORKING_DIRECTORY ${QT_SOURCE_DIR})
    execute_process(COMMAND rm ${QT_SOURCE_DIR}/lib/cmake/Qt5Gui/Qt5Gui_QGtk3ThemePlugin.cmake
            WORKING_DIRECTORY ${QT_SOURCE_DIR})
    execute_process(COMMAND touch ${QT_SOURCE_DIR}/lib/cmake/Qt5Gui/Qt5Gui_QGtk3ThemePlugin.cmake
            WORKING_DIRECTORY ${QT_SOURCE_DIR})
ENDIF()
