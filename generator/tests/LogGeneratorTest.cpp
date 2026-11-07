#include "LogGenerator.hpp"

#include <gtest/gtest.h>

#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std::chrono;

namespace {

// 24 Oct 2026, 10:15:32 UTC
const sys_seconds kStart =
    sys_days{year{2026} / month{10} / day{24}} + hours{10} + minutes{15} + seconds{32};

// Matches: 10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET /api/users HTTP/1.1" 200 1024 1000
const std::regex kLineRe(
    R"RE(^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}) - - \[(\d{2}/[A-Z][a-z]{2}/\d{4}:\d{2}:\d{2}:\d{2}) \+0000\] )RE"
    R"RE("(GET|POST|PUT|DELETE) (\S+) HTTP/1\.1" (\d{3}) (\d+|-) "([^"]*)" "([^"]*)" (\d+)$)RE");

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in{text};
    for (std::string line; std::getline(in, line);) {
        lines.push_back(line);
    }
    return lines;
}

std::string generate(std::uint64_t lines) {
    std::ostringstream out;
    logproc::generateLog(out, {.lines = lines, .start = kStart});
    return out.str();
}

}  // namespace

TEST(LogGenTest, ZeroLinesProducesEmptyOutput) {
    EXPECT_TRUE(generate(0).empty());
}

TEST(LogGenTest, WritesExactlyTheRequestedNumberOfLines) {
    EXPECT_EQ(splitLines(generate(500)).size(), 500u);
}

TEST(LogGenTest, RepeatedRunsProduceIdenticalOutput) {
    EXPECT_EQ(generate(200), generate(200));
}

TEST(LogGenTest, FirstLineHasStartTimestamp) {
    const auto lines = splitLines(generate(1));
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].find("[24/Oct/2026:10:15:32 +0000]"), std::string::npos) << lines[0];
}

TEST(LogGenTest, TimestampAdvancesOneSecondPerLine) {
    const auto lines = splitLines(generate(3));
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_NE(lines[0].find(":32 +0000]"), std::string::npos) << lines[0];
    EXPECT_NE(lines[1].find(":33 +0000]"), std::string::npos) << lines[1];
    EXPECT_NE(lines[2].find(":34 +0000]"), std::string::npos) << lines[2];
}

TEST(LogGenTest, EveryLineMatchesApacheStyleFormat) {
    for (const auto& line : splitLines(generate(1000))) {
        std::smatch m;
        ASSERT_TRUE(std::regex_match(line, m, kLineRe)) << "bad line: " << line;
    }
}

TEST(LogGenTest, FieldsAreInExpectedRanges) {
    const std::set<int> allowed{200, 201, 301, 304, 404, 500};

    for (const auto& line : splitLines(generate(1000))) {
        std::smatch m;
        ASSERT_TRUE(std::regex_match(line, m, kLineRe)) << line;

        const int status = std::stoi(m[5]);
        const unsigned long latency = std::stoul(m[9]);

        EXPECT_TRUE(allowed.contains(status)) << "status " << status;
        EXPECT_GE(latency, 1000ul);
        EXPECT_LE(latency, 10800ul);
    }
}

TEST(LogGenTest, AllStatusCodesAppear) {
    std::set<int> seen;
    for (const auto& line : splitLines(generate(100))) {
        std::smatch m;
        ASSERT_TRUE(std::regex_match(line, m, kLineRe));
        seen.insert(std::stoi(m[5]));
    }
    EXPECT_EQ(seen, (std::set<int>{200, 201, 301, 304, 404, 500}));
}

TEST(LogGenTest, LineGeneratorMatchesGenerateLog) {
    const auto lines = splitLines(generate(10));

    logproc::LineGenerator gen{kStart};
    for (const auto& expected : lines) {
        EXPECT_EQ(gen.next(), expected);
    }
}