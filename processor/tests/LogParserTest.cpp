#include "LogGenerator.hpp"
#include "LogParser.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string_view>

using namespace std::chrono;
using logproc::ParseError;
using logproc::parseLine;

namespace {

constexpr std::string_view kValid =
    R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET /api/users HTTP/1.1" 200 1024 1000)";

// Returns the error for a line that should fail, or nullopt if it parsed.
std::optional<ParseError> errorOf(std::string_view line) {
    const auto result = parseLine(line);
    if (result) {
        return std::nullopt;
    }
    return result.error();
}

}  // namespace

TEST(LogParserTest, ParsesEveryFieldOfValidLine) {
    const auto result = parseLine(kValid);
    ASSERT_TRUE(result.has_value());

    const auto& r = *result;
    EXPECT_EQ(r.timestamp, sys_days{year{2026} / month{10} / day{24}} + hours{10} + minutes{15} + seconds{32});
    EXPECT_EQ(r.method, "GET");
    EXPECT_EQ(r.path, "/api/users");
    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.bytes, 1024u);
    EXPECT_EQ(r.latencyUs, 1000u);
}

TEST(LogParserTest, ParsesEveryLineFromGenerator) {
    const sys_seconds start =
        sys_days{year{2026} / month{10} / day{24}} + hours{10} + minutes{15} + seconds{32};
    logproc::LineGenerator gen{start};

    for (int i = 0; i < 1000; ++i) {
        const std::string line = gen.next();
        ASSERT_TRUE(parseLine(line).has_value()) << "failed on: " << line;
    }
}

TEST(LogParserTest, RejectsEmptyLine) {
    EXPECT_EQ(errorOf(""), ParseError::MissingField);
}

TEST(LogParserTest, RejectsTruncatedLine) {
    EXPECT_EQ(errorOf("10.0.0.1 - -"), ParseError::MissingField);
}

TEST(LogParserTest, RejectsMissingLatency) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET / HTTP/1.1" 200 1024)"),
              ParseError::MissingField);
}

TEST(LogParserTest, RejectsUnknownMonth) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Foo/2026:10:15:32 +0000] "GET / HTTP/1.1" 200 1024 1000)"),
              ParseError::BadTimestamp);
}

TEST(LogParserTest, RejectsImpossibleDate) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [31/Feb/2026:10:15:32 +0000] "GET / HTTP/1.1" 200 1024 1000)"),
              ParseError::BadTimestamp);
}

TEST(LogParserTest, RejectsHourOutOfRange) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:25:15:32 +0000] "GET / HTTP/1.1" 200 1024 1000)"),
              ParseError::BadTimestamp);
}

TEST(LogParserTest, RejectsNonUtcZone) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0200] "GET / HTTP/1.1" 200 1024 1000)"),
              ParseError::BadTimestamp);
}

TEST(LogParserTest, RejectsUnquotedRequest) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] GET / HTTP/1.1 200 1024 1000)"),
              ParseError::BadRequest);
}

TEST(LogParserTest, RejectsUnknownProtocol) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET / FTP/1.1" 200 1024 1000)"),
              ParseError::BadRequest);
}

TEST(LogParserTest, RejectsStatusOutOfRange) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET / HTTP/1.1" 70000 1024 1000)"),
              ParseError::BadStatus);
}

TEST(LogParserTest, RejectsNegativeStatus) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET / HTTP/1.1" -1 1024 1000)"),
              ParseError::BadStatus);
}

TEST(LogParserTest, RejectsNonNumericBytes) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET / HTTP/1.1" 200 1k 1000)"),
              ParseError::BadBytes);
}

TEST(LogParserTest, RejectsUnitSuffixOnLatency) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET / HTTP/1.1" 200 1024 10ms)"),
              ParseError::BadLatency);
}

TEST(LogParserTest, RejectsExtraFieldAfterLatency) {
    EXPECT_EQ(errorOf(R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET / HTTP/1.1" 200 1024 1000 extra)"),
              ParseError::BadLatency);
}