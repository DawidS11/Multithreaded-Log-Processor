#include "CliOptions.hpp"

#include <charconv>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace logproc {
namespace {

std::unexpected<std::string> fail(std::string message) {
    return std::unexpected<std::string>{std::move(message)};
}

// Parses a positive integer. Returns nothing if the text is not a number or is zero.
std::optional<std::size_t> parsePositive(const std::string& text) {
    std::size_t value = 0;
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), last, value);
    if (ec != std::errc{} || ptr != last || value == 0) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

std::expected<CliOptions, std::string> parseCliArgs(const std::vector<std::string>& args) {
    // The file must come first. A missing file or an option in that position is an error.
    if (args.empty() || args[0].starts_with("--")) {
        return fail("missing input file");
    }

    CliOptions opts;
    opts.path = args[0];

    // Everything after the file must be an option followed by its value.
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--workers" || arg == "--batch") {
            if (i + 1 >= args.size()) {
                return fail(arg + " needs a number");
            }
            const std::string& text = args[++i];
            const auto value = parsePositive(text);
            if (!value) {
                return fail("invalid value '" + text + "' for " + arg);
            }
            if (arg == "--workers") {
                opts.workers = *value;
            } else {
                opts.batchSize = *value;
            }
        } else {
            return fail("unexpected argument '" + arg + "'");
        }
    }

    return opts;
}

}  // namespace logproc