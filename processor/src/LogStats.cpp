#include "LogStats.hpp"

#include <algorithm>
#include <istream>
#include <map>
#include <ostream>
#include <print>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "BoundedQueue.hpp"

namespace logproc {
namespace {

std::string normalizePath(std::string_view path) {
    const auto queryPos = path.find('?');
    const std::string_view base = queryPos == std::string_view::npos ? path : path.substr(0, queryPos);
    if (base.empty()) {
        return "/";
    }

    std::string normalized;
    normalized.reserve(base.size());
    bool seenSegment = false;

    std::size_t start = 0;
    while (start < base.size()) {
        const std::size_t slashPos = base.find('/', start);
        const std::string_view segment = slashPos == std::string_view::npos
            ? base.substr(start)
            : base.substr(start, slashPos - start);

        if (!segment.empty()) {
            if (!seenSegment) {
                normalized.push_back('/');
                seenSegment = true;
            } else {
                normalized.push_back('/');
            }

            const bool isNumeric = std::all_of(segment.begin(), segment.end(), [](unsigned char ch) {
                return ch >= '0' && ch <= '9';
            });

            if (isNumeric) {
                normalized += ":id";
            } else {
                normalized.append(segment.begin(), segment.end());
            }
        }

        if (slashPos == std::string_view::npos) {
            break;
        }
        start = slashPos + 1;
    }

    return normalized.empty() ? "/" : normalized;
}

}  // namespace

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
    ++byEndpoint[normalizePath(record.path)];
    const auto minuteKey = std::chrono::floor<std::chrono::minutes>(record.timestamp)
                              .time_since_epoch()
                              .count();
    ++byMinute[minuteKey];
    totalBytes += record.bytes;
    totalLatencyUs += record.latencyUs;
    maxLatencyUs = std::max(maxLatencyUs, record.latencyUs);
}

void Stats::merge(const Stats& other) {
    lines += other.lines;
    parsed += other.parsed;
    malformed += other.malformed;
    for (const auto& [key, val] : other.errors) {
        errors[key] += val;
    }
    for (const auto& [key, val] : other.byStatus) {
        byStatus[key] += val;
    }
    for (const auto& [path, count] : other.byEndpoint) {
        byEndpoint[path] += count;
    }
    for (const auto& [minute, count] : other.byMinute) {
        byMinute[minute] += count;
    }
    totalBytes += other.totalBytes;
    totalLatencyUs += other.totalLatencyUs;
    maxLatencyUs = std::max(maxLatencyUs, other.maxLatencyUs);
}

Stats analyze(std::istream& in, const std::size_t workerCount, const std::size_t batchSize) {
    if (workerCount == 0) {
        throw std::invalid_argument{"workerCount must be at least 1"};
    }
    BoundedQueue<std::vector<std::string>> queue{workerCount * 2};
    // One Stats per worker, sized before any thread starts. The workers hold references
    // into this vector, so it must never resize while they run.
    std::vector<Stats> perWorker(workerCount);
    std::vector<std::jthread> workers;
    workers.reserve(workerCount);

    for (std::size_t i = 0; i < workerCount; ++i) {
        workers.emplace_back([&queue, &stats = perWorker[i]] {
            while (auto batch = queue.pop()) {
                for (const auto& line : *batch) {
                    stats.addLine(line);
                }
            }
        });
    }

    std::vector<std::string> batch;
    batch.reserve(batchSize);
    auto pushBatch = [&] {
        if (!queue.push(std::move(batch))) {
            throw std::logic_error("queue rejected a batch");
        }
        // The moved-from vector is valid but empty-ish. Clear it to be sure.
        batch.clear();
        batch.reserve(batchSize);
    };

    // Main thread creates batches and pushes them to the queue.
    try {
        for (std::string line; std::getline(in, line);) {
            batch.push_back(std::move(line));
            if (batch.size() == batchSize) {
                pushBatch();
            }
        }
        // The last batch is usually smaller than batchSize.
        if (!batch.empty()) {
            pushBatch();
        }
    } catch (...) {
        queue.close();
        throw;
    }

    queue.close();
    // Join every worker.
    workers.clear();

    Stats total;
    for (const auto& s : perWorker) {
        total.merge(s);
    }

    return total;
}

Stats analyzeWithoutThreads(std::istream& in) {
    Stats stats;
    for (std::string line; std::getline(in, line);) {
        stats.addLine(line);
    }
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

    
    if (!stats.byMinute.empty()) {
        std::println(out, "\nRequests per minute:");
        for (const auto& [minute, count] : stats.byMinute) {
            std::println(out, "  {}  {}", minute, count);
        }
    }
    

    if (!stats.byEndpoint.empty()) {
        std::vector<std::pair<std::string, std::uint64_t>> endpoints(stats.byEndpoint.begin(), stats.byEndpoint.end());
        std::sort(endpoints.begin(), endpoints.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.second != rhs.second) {
                return lhs.second > rhs.second;
            }
            return lhs.first < rhs.first;
        });

        std::println(out, "\nTop endpoints:");
        const auto limit = std::min<std::size_t>(10, endpoints.size());
        for (std::size_t i = 0; i < limit; ++i) {
            std::println(out, "  {:<30} {}", endpoints[i].first, endpoints[i].second);
        }
    }

    if (!stats.errors.empty()) {
        std::println(out, "\nMalformed lines by reason:");
        for (const auto& [error, count] : stats.errors) {
            std::println(out, "  {:<14} {}", toString(error), count);
        }
    }
}

}  // namespace logproc