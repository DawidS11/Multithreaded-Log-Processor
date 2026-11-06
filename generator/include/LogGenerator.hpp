#pragma once

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>

namespace logproc {

// Settings for generate_log().
struct GenOptions {
    std::uint64_t lines;  // how many lines to write
    std::chrono::sys_seconds start{std::chrono::seconds{1'761'300'000}};  // timestamp of the first line
};

// Writes opts.lines access log lines to out, one per line.
void generate_log(std::ostream& out, const GenOptions& opts);

// Produces log lines one at a time.
class LineGenerator {
public:
    explicit LineGenerator(std::chrono::sys_seconds start) noexcept;

    // Returns the next line without a trailing newline.
    // [[nodiscard]]: calling it and ignoring the result is almost certainly a bug.
    [[nodiscard]] std::string next();

private:
    std::chrono::sys_seconds start_;  // timestamp of line 0
    std::uint64_t index_ = 0;         // number of lines produced so far
};

}  // namespace logproc