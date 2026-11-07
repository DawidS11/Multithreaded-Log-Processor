#include "LogGenerator.hpp"

#include <array>
#include <format>
#include <print>
#include <string>
#include <string_view>

namespace logproc {
namespace {

// Fixed vocabulary that the lines cycle through.
constexpr std::array<std::string_view, 4> kMethods{"GET", "POST", "PUT", "DELETE"};
constexpr std::array<std::string_view, 8> kPaths{"/api/users", "/api/orders", "/api/products",
                                                 "/api/search", "/login", "/logout",
                                                 "/static/app.js", "/index.html"};
constexpr std::array<int, 6> kStatuses{200, 201, 301, 304, 404, 500};
constexpr std::array<std::string_view, 3> kReferrers{
    "-", "https://example.com/", "https://search.example.org/?q=logs"};
constexpr std::array<std::string_view, 3> kAgents{
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0)",
    "curl/8.4.0",
    "Mozilla/5.0 (X11; Linux x86_64) Firefox/120.0"};

}  // namespace

LineGenerator::LineGenerator(std::chrono::sys_seconds start) noexcept : start{start} {}

std::string LineGenerator::next() {
    // Use the current index for every field, then advance it.
    const std::uint64_t i = index++;

    // Hardcoded dense-rates traffic: 300,000 requests per minute (~5,000/sec).
    const auto timestamp = start + std::chrono::seconds{i / 5000};

    // Client IP cycles through 10.0.0.1 to 10.0.0.254.
    const std::string ip = std::format("10.0.0.{}", i % 254 + 1);

    // Each field cycles through its own list. Different list lengths mean the combinations vary.
    const std::string_view method = kMethods[i % kMethods.size()];
    const std::string_view path = kPaths[i % kPaths.size()];
    const int status = kStatuses[i % kStatuses.size()];
    const std::string_view referrer = kReferrers[i % kReferrers.size()];
    const std::string_view agent = kAgents[i % kAgents.size()];

    // Fixed response size, in bytes. Apache writes "-" when no body was sent, such as a 304.
    const std::string bytes = status == 304 ? "-" : "1024";

    // Latency cycles from 1.0 ms to 10.8 ms, stored in microseconds (Apache's %D field).
    const std::uint64_t latency = 1000 + (i % 50) * 200;

    // Apache combined log format, with the latency (%D) appended as the last field.
    return std::format("{} - - [{:%d/%b/%Y:%H:%M:%S} +0000] \"{} {} HTTP/1.1\" {} {} \"{}\" \"{}\" {}",
                       ip, timestamp, method, path, status, bytes, referrer, agent, latency);
}

void generateLog(std::ostream& out, const GenOptions& opts) {
    // Line example:
    // 10.0.0.5 - - [24/Oct/2025:10:00:04 +0000] "GET /login HTTP/1.1" 500 1024 "-" "curl/8.4.0" 1800

    LineGenerator gen{opts.start};

    // Write the requested number of lines.
    for (std::uint64_t i = 0; i < opts.lines; ++i) {
        std::println(out, "{}", gen.next());
    }
}

}  // namespace logproc