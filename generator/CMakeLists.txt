add_library(logproc_gen src/LogGenerator.cpp)
target_include_directories(logproc_gen PUBLIC include)
target_compile_options(logproc_gen PRIVATE -Wall -Wextra -Wpedantic)

add_executable(loggen tools/LogGeneratorCli.cpp)
target_link_libraries(loggen PRIVATE logproc_gen)

add_executable(loggen_tests tests/LogGeneratorTest.cpp)
target_link_libraries(loggen_tests PRIVATE logproc_gen GTest::gtest_main)
gtest_discover_tests(loggen_tests)