cmake_minimum_required(VERSION 3.19.0 FATAL_ERROR)

project(radiation_necrosis)

find_package(BioDynaMo REQUIRED)
include(${BDM_USE_FILE})
bdm_add_executable(radiation_necrosis
    HEADERS src/radiation_necrosis.h
    SOURCES src/radiation_necrosis.cc
    LIBRARIES ${BDM_REQUIRED_LIBRARIES})
