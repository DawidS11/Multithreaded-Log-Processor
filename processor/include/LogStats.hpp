#pragma once

#include "LogParser.hpp"

#include <chrono>
#include <cstdint>
#include <istream>
#include <map>
#include <ostream>
#include <string_view>

namespace logproc {

// Running totals for a set of log lines.
struct Stats {
    std::uint64_t lines = 0;       // every line seen
    std::uint64_t parsed = 0;      // lines that parsed successfully
    std::uint64_t malformed = 0;   // lines that failed to parse
    std::map<ParseError, std::uint64_t> errors;       // malformed lines, by reason
    std::map<std::uint16_t, std::uint64_t> byStatus;  // parsed lines, by status code
    std::map<std::int64_t, std::uint64_t> byMinute;    // requests grouped by minute
    std::map<std::string, std::uint64_t> byEndpoint;   // normalized routes, e.g. /users/:id
    std::uint64_t totalBytes = 0;
    std::uint64_t totalLatencyUs = 0;
    std::uint32_t maxLatencyUs = 0;

    // Parses one line and updates the totals.
    void addLine(std::string_view line);

    // Merge per-worked stats.
    void merge(const Stats& other);
};

// Reads every line from the stream and returns the totals.
// Parses on workerCount threads, each with its own Stats, then merges them.
Stats analyze(std::istream& in, const std::size_t workerCount = 1,
              const std::size_t batchSize = 1000);

// Reads every line from the stream and returns the totals.
// Processes line by line with one thread.
Stats analyzeWithoutThreads(std::istream& in);

// Writes a human-readable summary.
void printReport(std::ostream& out, const Stats& stats);

}  // namespace logproc