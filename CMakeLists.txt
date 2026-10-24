cmake_minimum_required(VERSION 3.20)
project(logproc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Log generator library
add_library(logproc_gen src/LogGenerator.cpp)
target_include_directories(logproc_gen PUBLIC include)
target_compile_options(logproc_gen PRIVATE -Wall -Wextra -Wpedantic)

# Parser and statistics library
add_library(logproc_core src/LogParser.cpp src/LogStats.cpp)
target_include_directories(logproc_core PUBLIC include)
target_compile_options(logproc_core PRIVATE -Wall -Wextra -Wpedantic)

# Command-line tools
add_executable(loggen tools/LogGeneratorCli.cpp)
target_link_libraries(loggen PRIVATE logproc_gen)

add_executable(logproc tools/LogProcessorCli.cpp)
target_link_libraries(logproc PRIVATE logproc_core)

# Tests
enable_testing()
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(loggen_tests tests/LogGeneratorTest.cpp)
target_link_libraries(loggen_tests PRIVATE logproc_gen GTest::gtest_main)
gtest_discover_tests(loggen_tests)

add_executable(logparser_tests tests/LogParserTest.cpp)
target_link_libraries(logparser_tests PRIVATE logproc_core logproc_gen GTest::gtest_main)
gtest_discover_tests(logparser_tests)