#include "CliOptions.hpp"
#include "LogStats.hpp"

#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    const auto parsed = logproc::parseCliArgs(args);
    if (!parsed) {
        std::println(std::cerr, "logproc: {}", parsed.error());
        std::println(std::cerr, "usage: logproc [--workers N] <access.log>");
        return 2;
    }

    std::ifstream in{parsed->path};
    if (!in) {
        std::println(std::cerr, "logproc: cannot open '{}'", parsed->path);
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();
    const auto stats = logproc::analyze(in, parsed->workers);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::println(std::cerr, "{} workers: {:.3f} s", parsed->workers,
        std::chrono::duration<double>(elapsed).count());

    logproc::printReport(std::cout, stats);
    return 0;
}