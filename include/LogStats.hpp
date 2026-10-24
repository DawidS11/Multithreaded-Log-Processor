#pragma once

#include "LogParser.hpp"

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
    std::map<ParseError, std::uint64_t> errors;     // malformed lines, by reason
    std::map<std::uint16_t, std::uint64_t> by_status;  // parsed lines, by status code
    std::uint64_t total_bytes = 0;
    std::uint64_t total_latency_us = 0;
    std::uint32_t max_latency_us = 0;

    // Parses one line and updates the totals.
    void add_line(std::string_view line);
};

// Reads every line from the stream and returns the totals. Single-threaded.
Stats analyze(std::istream& in);

// Writes a human-readable summary.
void print_report(std::ostream& out, const Stats& stats);

}  // namespace logproc