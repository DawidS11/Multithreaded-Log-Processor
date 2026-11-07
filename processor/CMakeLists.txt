find_package(Threads REQUIRED)

# Parser, statistics, and CLI argument parsing, shared by the tool and the tests
add_library(logproc_core
  src/LogParser.cpp
  src/LogStats.cpp
  src/CliOptions.cpp)
target_include_directories(logproc_core PUBLIC include)
target_compile_options(logproc_core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(logproc_core PUBLIC Threads::Threads)

add_executable(logproc tools/LogProcessorCli.cpp)
target_link_libraries(logproc PRIVATE logproc_core)

# The parser tests use LineGenerator for the round-trip test
add_executable(logparser_tests tests/LogParserTest.cpp)
target_link_libraries(logparser_tests PRIVATE logproc_core logproc_gen GTest::gtest_main)
gtest_discover_tests(logparser_tests)

# BoundedQueue is header-only with no parser dependency
add_executable(queue_tests tests/BoundedQueueTest.cpp)
target_include_directories(queue_tests PRIVATE include)
target_link_libraries(queue_tests PRIVATE GTest::gtest_main Threads::Threads)
gtest_discover_tests(queue_tests)

# LogStats pipeline tests
add_executable(pipeline_tests tests/LogStatsTest.cpp)
target_link_libraries(pipeline_tests PRIVATE logproc_core logproc_gen GTest::gtest_main)
gtest_discover_tests(pipeline_tests)

# CLI argument parsing tests
add_executable(cli_tests tests/CliOptionsTest.cpp)
target_link_libraries(cli_tests PRIVATE logproc_core GTest::gtest_main)
gtest_discover_tests(cli_tests)

# End-to-end smoke test.
add_test(NAME logproc_smoke
  COMMAND sh -c "\"$<TARGET_FILE:loggen>\" 20000 > smoke.log && \"$<TARGET_FILE:logproc>\" --workers 2 smoke.log"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})