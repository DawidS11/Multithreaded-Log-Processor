#include "LogGenerator.hpp"
#include "LogStats.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>

namespace {

constexpr std::uint64_t kLines = 100'000;

std::string makeLog(std::uint64_t lines) {
    std::ostringstream out;
    logproc::generateLog(out, {.lines = lines});
    return out.str();
}

// Runs the full pipeline and returns the printed report, so two runs can be compared as text.
std::string reportFor(const std::string& log, std::size_t workers) {
    std::istringstream in{log};
    const auto stats = logproc::analyze(in, workers);
    std::ostringstream report;
    logproc::printReport(report, stats);
    return report.str();
}

}  // namespace

TEST(LogStatsPipelineTest, CountsEveryLineWithSeveralWorkers) {
    std::istringstream in{makeLog(kLines)};
    const auto stats = logproc::analyze(in, 4);

    EXPECT_EQ(stats.lines, kLines);
    EXPECT_EQ(stats.parsed, kLines);
    EXPECT_EQ(stats.malformed, 0u);
}

TEST(LogStatsPipelineTest, ReportIsIdenticalForAnyWorkerCount) {
    const auto log = makeLog(kLines);
    const auto expected = reportFor(log, 1);

    for (std::size_t workers : {2, 4, 8}) {
        EXPECT_EQ(reportFor(log, workers), expected) << workers << " workers";
    }
}

TEST(LogStatsPipelineTest, CountsMalformedLineAcrossWorkers) {
    std::string log = makeLog(kLines);
    log += "garbage\n";  // one bad line after the last batch boundary

    std::istringstream in{log};
    const auto stats = logproc::analyze(in, 4);

    EXPECT_EQ(stats.lines, kLines + 1);
    EXPECT_EQ(stats.parsed, kLines);
    EXPECT_EQ(stats.malformed, 1u);
}

TEST(LogStatsPipelineTest, GroupsRequestsByMinute) {
    std::istringstream in{
        R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET /one HTTP/1.1" 200 10 100
10.0.0.2 - - [24/Oct/2026:10:15:59 +0000] "GET /two HTTP/1.1" 200 10 100
10.0.0.3 - - [24/Oct/2026:10:16:00 +0000] "GET /three HTTP/1.1" 200 10 100
10.0.0.4 - - [24/Oct/2026:10:17:30 +0000] "GET /four HTTP/1.1" 200 10 100
)"};

    const auto stats = logproc::analyzeWithoutThreads(in);

    EXPECT_EQ(stats.byMinute.size(), 3u);

    const auto minute1 = std::chrono::floor<std::chrono::minutes>(
        std::chrono::sys_days{std::chrono::year{2026} / std::chrono::month{10} / std::chrono::day{24}} +
        std::chrono::hours{10} + std::chrono::minutes{15});
    const auto minute2 = std::chrono::floor<std::chrono::minutes>(
        std::chrono::sys_days{std::chrono::year{2026} / std::chrono::month{10} / std::chrono::day{24}} +
        std::chrono::hours{10} + std::chrono::minutes{16});
    const auto minute3 = std::chrono::floor<std::chrono::minutes>(
        std::chrono::sys_days{std::chrono::year{2026} / std::chrono::month{10} / std::chrono::day{24}} +
        std::chrono::hours{10} + std::chrono::minutes{17});

    const auto it1 = stats.byMinute.find(minute1.time_since_epoch().count());
    const auto it2 = stats.byMinute.find(minute2.time_since_epoch().count());
    const auto it3 = stats.byMinute.find(minute3.time_since_epoch().count());

    ASSERT_NE(it1, stats.byMinute.end());
    ASSERT_NE(it2, stats.byMinute.end());
    ASSERT_NE(it3, stats.byMinute.end());
    EXPECT_EQ(it1->second, 2u);
    EXPECT_EQ(it2->second, 1u);
    EXPECT_EQ(it3->second, 1u);
}

TEST(LogStatsPipelineTest, NormalizesNumericPathSegments) {
    std::istringstream in{
        R"(10.0.0.1 - - [24/Oct/2026:10:15:32 +0000] "GET /users/42 HTTP/1.1" 200 10 100
10.0.0.2 - - [24/Oct/2026:10:15:59 +0000] "GET /users/99 HTTP/1.1" 200 10 100
10.0.0.3 - - [24/Oct/2026:10:16:00 +0000] "GET /users/42/profile HTTP/1.1" 200 10 100
10.0.0.4 - - [24/Oct/2026:10:17:30 +0000] "GET /users/42?expand=true HTTP/1.1" 200 10 100
10.0.0.5 - - [24/Oct/2026:10:17:45 +0000] "GET /health HTTP/1.1" 200 10 100
)"};

    const auto stats = logproc::analyzeWithoutThreads(in);

    const auto usersIt = stats.byEndpoint.find("/users/:id");
    const auto profileIt = stats.byEndpoint.find("/users/:id/profile");
    const auto healthIt = stats.byEndpoint.find("/health");

    ASSERT_NE(usersIt, stats.byEndpoint.end());
    ASSERT_NE(profileIt, stats.byEndpoint.end());
    ASSERT_NE(healthIt, stats.byEndpoint.end());
    EXPECT_EQ(usersIt->second, 3u);
    EXPECT_EQ(profileIt->second, 1u);
    EXPECT_EQ(healthIt->second, 1u);
}