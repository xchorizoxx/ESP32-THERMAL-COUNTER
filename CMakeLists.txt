# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH not defined. Please set up the ESP-IDF environment.")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(DetectorPuerta)
