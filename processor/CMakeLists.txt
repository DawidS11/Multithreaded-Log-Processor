find_package(Threads REQUIRED)

add_library(logproc_core src/LogParser.cpp src/LogStats.cpp)
target_include_directories(logproc_core PUBLIC include)
target_compile_options(logproc_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(logproc tools/LogProcessorCli.cpp)
target_link_libraries(logproc PRIVATE logproc_core)

# The parser tests use LineGenerator for the round-trip test, so they link to logproc_gen too
add_executable(logparser_tests tests/LogParserTest.cpp)
target_link_libraries(logparser_tests PRIVATE logproc_core logproc_gen GTest::gtest_main)
gtest_discover_tests(logparser_tests)

# BoundedQueueTest tests
add_executable(queue_tests tests/BoundedQueueTest.cpp)
target_include_directories(queue_tests PRIVATE include)
target_link_libraries(queue_tests PRIVATE GTest::gtest_main Threads::Threads)
gtest_discover_tests(queue_tests)