#include "CliOptions.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <thread>

using logproc::parseCliArgs;

TEST(CliOptionsTest, FileAloneUsesHardwareConcurrencyWorkers) {
    const auto r = parseCliArgs({"access.log"});
    const std::size_t workers = std::thread::hardware_concurrency();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->path, "access.log");
    EXPECT_EQ(r->workers, workers);
}

TEST(CliOptionsTest, WorkersAfterFile) {
    const auto r = parseCliArgs({"access.log", "--workers", "4"});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->path, "access.log");
    EXPECT_EQ(r->workers, 4u);
}

TEST(CliOptionsTest, WorkersBeforeFile) {
    const auto r = parseCliArgs({"--workers", "8", "access.log"});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->path, "access.log");
    EXPECT_EQ(r->workers, 8u);
}

TEST(CliOptionsTest, RejectsZeroWorkers) {
    const auto r = parseCliArgs({"--workers", "0", "access.log"});
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().find("invalid worker count"), std::string::npos);
}

TEST(CliOptionsTest, RejectsNonNumericWorkers) {
    EXPECT_FALSE(parseCliArgs({"--workers", "four", "access.log"}).has_value());
}

TEST(CliOptionsTest, RejectsNegativeWorkers) {
    EXPECT_FALSE(parseCliArgs({"--workers", "-2", "access.log"}).has_value());
}

TEST(CliOptionsTest, RejectsWorkersWithoutValue) {
    EXPECT_FALSE(parseCliArgs({"access.log", "--workers"}).has_value());
}

TEST(CliOptionsTest, RejectsMissingFile) {
    EXPECT_FALSE(parseCliArgs({}).has_value());
    EXPECT_FALSE(parseCliArgs({"--workers", "2"}).has_value());
}

TEST(CliOptionsTest, RejectsTwoFiles) {
    EXPECT_FALSE(parseCliArgs({"a.log", "b.log"}).has_value());
}

TEST(CliOptionsTest, RejectsUnknownOption) {
    EXPECT_FALSE(parseCliArgs({"--verbose", "access.log"}).has_value());
}