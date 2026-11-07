#include "LogStats.hpp"

#include <algorithm>
#include <print>
#include <string>

#include "BoundedQueue.hpp"

namespace logproc {

void Stats::addLine(std::string_view line) {
    ++lines;

    const auto result = parseLine(line);
    if (!result) {
        ++malformed;
        ++errors[result.error()];
        return;
    }

    const LogRecord& record = *result;
    ++parsed;
    ++byStatus[record.status];
    totalBytes += record.bytes;
    totalLatencyUs += record.latencyUs;
    maxLatencyUs = std::max(maxLatencyUs, record.latencyUs);
}

Stats analyze(std::istream& in) {
    constexpr std::size_t kBatchSize = 1000;
    BoundedQueue<std::vector<std::string>> queue{1};
    std::vector<std::string> batch;
    batch.reserve(kBatchSize);
    Stats stats;

    // Send the current batch through the queue, then process it.
    auto flush = [&]() {
        if (!queue.push(std::move(batch))) {
            throw std::logic_error("queue rejected a batch");
        }
        // The moved-from vector is valid but empty-ish. Clear it to be sure.
        batch.clear();
        batch.reserve(kBatchSize);

        // The same thread pops from the queue and adds line.
        auto ready = queue.pop();
        for (const auto& line : *ready) {
            stats.addLine(line);
        }
    };

    for (std::string line; std::getline(in, line);) {
        batch.push_back(std::move(line));
        if (batch.size() == kBatchSize) {
            flush();
        }
    }
    // The last batch is usually smaller than kBatchSize.
    if (!batch.empty()) {
        flush();
    }

    queue.close();

    return stats;
}

void printReport(std::ostream& out, const Stats& stats) {
    std::println(out, "Lines read:       {}", stats.lines);
    std::println(out, "Parsed:           {}", stats.parsed);
    std::println(out, "Malformed:        {}", stats.malformed);

    if (stats.parsed > 0) {
        const double avgMs = stats.totalLatencyUs / 1000.0 / static_cast<double>(stats.parsed);
        std::println(out, "Total bytes:      {}", stats.totalBytes);
        std::println(out, "Average latency:  {:.2f} ms", avgMs);
        std::println(out, "Max latency:      {:.2f} ms", stats.maxLatencyUs / 1000.0);
    }

    std::println(out, "\nStatus codes:");
    for (const auto& [status, count] : stats.byStatus) {
        std::println(out, "  {:>3}  {}", status, count);
    }

    if (!stats.errors.empty()) {
        std::println(out, "\nMalformed lines by reason:");
        for (const auto& [error, count] : stats.errors) {
            std::println(out, "  {:<14} {}", toString(error), count);
        }
    }
}

}  // namespace logproc