# -----------------------------------------------------------------------------
#
# Copyright (C) 2021 CERN & University of Surrey for the benefit of the
# BioDynaMo collaboration. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
#
# See the LICENSE file distributed with this work for details.
# See the NOTICE file distributed with this work for additional information
# regarding copyright ownership.
#
# -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.15)

# Demo: Standard C++ Replacements for ROOT
project(std_replacements_demo)

# Option to use standard C++ implementations instead of ROOT
option(BDM_USE_STD_RANDOM "Use standard C++ random instead of ROOT" OFF)
option(BDM_USE_BOOST_SERIALIZATION "Enable Boost serialization support" OFF)

# Find required packages
# Try to find Boost, but make it optional if building as part of BioDynaMo
if(BDM_USE_BOOST_SERIALIZATION OR BDM_USE_STD_RANDOM)
  find_package(Boost QUIET)
  if(BDM_USE_BOOST_SERIALIZATION)
    find_package(Boost QUIET COMPONENTS serialization)
  endif()
  
  if(NOT Boost_FOUND)
    message(WARNING "Boost not found. Trying to build without Boost support.")
    message(STATUS "To install Boost on Ubuntu/Debian: sudo apt-get install libboost-all-dev")
    message(STATUS "To install Boost on macOS: brew install boost")
    message(STATUS "To install Boost on CentOS/RHEL: sudo yum install boost-devel")
    set(BDM_USE_BOOST_SERIALIZATION OFF)
  else()
    message(STATUS "Found Boost version: ${Boost_VERSION}")
  endif()
endif()

# Set compiler flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add BioDynaMo (when building as part of BioDynaMo)
if(TARGET biodynamo)
  set(BDM_TARGET biodynamo)
else()
  # If building standalone, find BioDynaMo
  find_package(BioDynaMo REQUIRED)
  set(BDM_TARGET ${BioDynaMo_LIBRARIES})
endif()

# Configure preprocessor definitions
if(BDM_USE_STD_RANDOM)
  add_definitions(-DBDM_USE_STD_RANDOM=1)
  message(STATUS "Using standard C++ random number generation")
else()
  add_definitions(-DBDM_USE_STD_RANDOM=0)
  message(STATUS "Using ROOT random number generation")
endif()

if(BDM_USE_BOOST_SERIALIZATION AND Boost_FOUND)
  add_definitions(-DBDM_USE_BOOST_SERIALIZATION=1)
  message(STATUS "Using Boost serialization")
else()
  add_definitions(-DBDM_USE_BOOST_SERIALIZATION=0)
  message(STATUS "Using ROOT serialization")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

# Create demo executable
add_executable(demo_std_replacements
  demo_std_replacements.cc
)

# Link libraries
target_link_libraries(demo_std_replacements
  ${BDM_TARGET}
  pthread
)

# Link Boost if available
if(Boost_FOUND)
  target_link_libraries(demo_std_replacements ${Boost_LIBRARIES})
endif()

# Add test executable (if building with BioDynaMo testing)
if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  
  add_executable(random_std_test
    ${CMAKE_SOURCE_DIR}/test/unit/core/util/random_std_test.cc
    ${CMAKE_SOURCE_DIR}/src/core/util/random_std.cc
  )
  
  target_link_libraries(random_std_test
    ${BDM_TARGET}
    GTest::gtest
    GTest::gtest_main
    pthread
  )
  
  # Link Boost if available
  if(Boost_FOUND)
    target_link_libraries(random_std_test ${Boost_LIBRARIES})
  endif()
  
  # Add the test
  add_test(NAME random_std_test COMMAND random_std_test)
endif()

# Install demo
install(TARGETS demo_std_replacements
  RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  BDM_USE_STD_RANDOM: ${BDM_USE_STD_RANDOM}")
message(STATUS "  Boost version: ${Boost_VERSION}")
message(STATUS "  Boost libraries: ${Boost_LIBRARIES}")
message(STATUS "")

# Instructions
message(STATUS "Build Instructions:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake .. [-DBDM_USE_STD_RANDOM=ON]")
message(STATUS "  make")
message(STATUS "  ./demo_std_replacements")
message(STATUS "")
