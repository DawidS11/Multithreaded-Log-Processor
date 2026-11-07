#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <vector>
#include <thread>

namespace logproc {

struct CliOptions {
    std::string path;
    std::size_t workers = std::thread::hardware_concurrency();
    std::size_t batchSize = 10000;
};

// Parses the arguments after the program name. Accepts --workers before or after the file.
// Returns the error message on failure.
std::expected<CliOptions, std::string> parseCliArgs(const std::vector<std::string>& args);

}  // namespace logproc