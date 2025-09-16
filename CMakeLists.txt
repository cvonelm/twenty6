# SPDX-License-Identifier: MIT
# 
# Main twenty6 CMake file
#
# Copyright (C) 2025 Technische Universität Dresden
# Christian von Elm <christian.von_elm@tu-dresden.de>

cmake_minimum_required(VERSION 3.11)
project(twenty6 VERSION 0.0.1)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(twenty6 INTERFACE)
target_include_directories(twenty6 INTERFACE include)

# Fuzzer

add_executable(fuzzer example/fuzzer.cpp)
target_link_libraries(fuzzer PRIVATE twenty6)


# Tests 
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
add_executable(tests tests/test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain twenty6)
catch_discover_tests(tests)
