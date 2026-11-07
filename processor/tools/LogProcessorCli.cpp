#include "LogStats.hpp"

#include <charconv>
#include <fstream>
#include <iostream>
#include <print>
#include <string_view>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::println(std::cerr, "usage: logproc <access.log> [--workers <num-workers>]");
        return 2;
    }

    std::ifstream in{argv[1]};
    if (!in) {
        std::println(std::cerr, "logproc: cannot open '{}'", argv[1]);
        return 1;
    }

    int numWorkers = std::thread::hardware_concurrency();
    if(argc == 4) {
        if (std::string_view{argv[2]} != "--workers") {
            std::println(std::cerr, "usage: logproc <access.log> [--workers <num-workers>]");
            return 2;
        }

        std::string_view workersStr{argv[3]};
        auto [ptr, ec] = std::from_chars(
            workersStr.data(), 
            workersStr.data() + workersStr.size(), 
            numWorkers
        );

        if (ec != std::errc() || ptr != workersStr.data() + workersStr.size() || numWorkers <= 0) {
            std::println(std::cerr, "logproc: invalid number of workers '{}'", argv[3]);
            return 2;
        }
    }

    const auto start = std::chrono::steady_clock::now();

    const auto stats = logproc::analyze(in, numWorkers);
    logproc::printReport(std::cout, stats);

    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::println(std::cerr, "{} workers: {:.3f} s", numWorkers,
             std::chrono::duration<double>(elapsed).count());
    return 0;
}