cmake_minimum_required(VERSION 3.11)
project(twenty6 VERSION 0.0.1)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(twenty6 INTERFACE)
target_include_directories(twenty6 INTERFACE include)


# Tests 
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)
add_executable(tests tests/test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain twenty6)
catch_discover_tests(tests)
