#include "LogGenerator.hpp"

#include <charconv>
#include <iostream>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(std::cerr, "usage: loggen <lines> > <log filename>");
        return 2;
    }

    const std::string text{argv[1]};
    logproc::GenOptions opts;

    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), opts.lines);

    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        std::println(std::cerr, "loggen: '{}' is not a number", text);
        return 2;
    }

    std::ios::sync_with_stdio(false);
    logproc::generateLog(std::cout, opts);
    return 0;
}