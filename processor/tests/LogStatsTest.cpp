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