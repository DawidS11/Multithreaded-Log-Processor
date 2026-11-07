#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace logproc {

struct LogRecord {
    std::chrono::sys_seconds timestamp;
    std::string method;
    std::string path;
    std::uint16_t status = 0;
    std::uint32_t bytes = 0;
    std::uint32_t latencyUs = 0;
};

enum class ParseError {
    MissingField,
    BadTimestamp,
    BadRequest,
    BadStatus,
    BadBytes,
    BadLatency,
};

// Short description of an error, used in the report.
[[nodiscard]] std::string_view toString(ParseError error) noexcept;

// Parses one Apache-style line as produced by LineGenerator.
[[nodiscard]] std::expected<LogRecord, ParseError> parseLine(std::string_view line);

}  // namespace logproc