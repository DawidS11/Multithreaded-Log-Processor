#pragma once

#include <queue>
#include <optional>

namespace logproc {

// A FIFO queue with a maximum size and a close flag.
template <typename T>
class BoundedQueue {
public:

    explicit BoundedQueue(std::size_t capacity)
        : capacity_{capacity} {}
    
    // Adds an item at the back. Returns false if the queue is full or closed,
    // in which case the item is not accepted.
    bool push(T batch) {
        if(data_.size() >= capacity_ || closed_) {
            return false;
        }
        data_.push(std::move(batch));
        return true;
    }

    // Removes and returns the oldest item, or nothing if the queue is empty.
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

    std::size_t size() const {
        return data_.size();
    }

private:
    std::queue<T> data_;
    std::size_t capacity_;
    bool closed_ = false;
};

} // namespace logproc