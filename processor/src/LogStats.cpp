#include "LogStats.hpp"

#include <algorithm>
#include <print>
#include <string>
#include <queue>

namespace logproc {

template <typename T>
class BoundedQueue {
public:

    explicit BoundedQueue(std::size_t capacity)
        : capacity_{capacity} {}
    
    bool push(T batch) {
        if(data_.size() >= capacity_ || closed_) {
            return false;
        }
        data_.push(std::move(batch));
        return true;
    }

    std::optional<T> pop() {
        if(data_.empty()) {
            return {};
        }
        T first = std::move(data_.front());
        data_.pop();
        return first;
    }

    void close() {
        closed_ = true;
    }

    bool closed() const {
        return closed_;
    }

private:
    std::queue<T> data_;
    std::size_t capacity_;
    bool closed_ = false;
};

void Stats::add_line(std::string_view line) {
    ++lines;

    const auto result = parse_line(line);
    if (!result) {
        ++malformed;
        ++errors[result.error()];
        return;
    }

    const LogRecord& record = *result;
    ++parsed;
    ++by_status[record.status];
    total_bytes += record.bytes;
    total_latency_us += record.latency_us;
    max_latency_us = std::max(max_latency_us, record.latency_us);
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
            stats.add_line(line);
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

void print_report(std::ostream& out, const Stats& stats) {
    std::println(out, "Lines read:       {}", stats.lines);
    std::println(out, "Parsed:           {}", stats.parsed);
    std::println(out, "Malformed:        {}", stats.malformed);

    if (stats.parsed > 0) {
        const double avg_ms = stats.total_latency_us / 1000.0 / static_cast<double>(stats.parsed);
        std::println(out, "Total bytes:      {}", stats.total_bytes);
        std::println(out, "Average latency:  {:.2f} ms", avg_ms);
        std::println(out, "Max latency:      {:.2f} ms", stats.max_latency_us / 1000.0);
    }

    std::println(out, "\nStatus codes:");
    for (const auto& [status, count] : stats.by_status) {
        std::println(out, "  {:>3}  {}", status, count);
    }

    if (!stats.errors.empty()) {
        std::println(out, "\nMalformed lines by reason:");
        for (const auto& [error, count] : stats.errors) {
            std::println(out, "  {:<14} {}", to_string(error), count);
        }
    }
}

}  // namespace logproc