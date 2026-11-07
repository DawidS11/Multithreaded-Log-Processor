#include "CliOptions.hpp"

#include <charconv>
#include <expected>
#include <string>
#include <vector>

namespace logproc {
namespace {

std::unexpected<std::string> fail(std::string message) {
    return std::unexpected<std::string>{std::move(message)};
}

}  // namespace

std::expected<CliOptions, std::string> parseCliArgs(const std::vector<std::string>& args) {
    CliOptions opts;
    bool haveFile = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--workers") {
            if (i + 1 >= args.size()) {
                return fail("--workers needs a number");
            }
            const std::string& text = args[++i];
            std::size_t n = 0;
            const char* last = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), last, n);
            if (ec != std::errc{} || ptr != last || n == 0) {
                return fail("invalid worker count '" + text + "'");
            }
            opts.workers = n;
        } else if (arg.starts_with("--")) {
            return fail("unknown option '" + arg + "'");
        } else if (!haveFile) {
            opts.path = arg;
            haveFile = true;
        } else {
            return fail("unexpected argument '" + arg + "'");
        }
    }

    if (!haveFile) {
        return fail("missing input file");
    }
    return opts;
}

}  // namespace logproc