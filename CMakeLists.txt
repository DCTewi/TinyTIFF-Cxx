
cmake_minimum_required(VERSION 3.20)

option(BUILD_TEST "build test project" OFF)

set(PROJECT_VERSION "1.0.0")

project(TinyTiffCxx VERSION ${PROJECT_VERSION})

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_library(tinytiff_cxx STATIC)

target_compile_features(tinytiff_cxx PRIVATE cxx_std_17)
target_include_directories(tinytiff_cxx PRIVATE ${CMAKE_SOURCE_DIR})
target_sources(tinytiff_cxx
    PRIVATE
    "tiff_cxx.h"
    "tiff_cxx.cpp"
)

if (BUILD_TEST)
    add_subdirectory("test")
endif()
