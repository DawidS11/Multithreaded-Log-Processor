#include "LogStats.hpp"

#include <fstream>
#include <iostream>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(std::cerr, "usage: logproc <access.log>");
        return 2;
    }

    std::ifstream in{argv[1]};
    if (!in) {
        std::println(std::cerr, "logproc: cannot open '{}'", argv[1]);
        return 1;
    }

    const auto stats = logproc::analyze(in);
    logproc::print_report(std::cout, stats);
    return 0;
}