cmake_minimum_required(VERSION 3.20)
project(logproc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Log generator library
add_library(logproc_gen src/LogGenerator.cpp)
target_include_directories(logproc_gen PUBLIC include)
target_compile_options(logproc_gen PRIVATE -Wall -Wextra -Wpedantic)

# Command-line tool
add_executable(loggen tools/LogGeneratorCli.cpp)
target_link_libraries(loggen PRIVATE logproc_gen)

# Tests
enable_testing()
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)

add_executable(loggen_tests tests/LogGeneratorTests.cpp)
target_link_libraries(loggen_tests PRIVATE logproc_gen GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(loggen_tests)