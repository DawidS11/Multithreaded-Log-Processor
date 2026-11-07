#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <queue>
#include <utility>

namespace logproc {

// A FIFO queue with a maximum size and a close flag.
template <typename T>
class BoundedQueue {
public:

    explicit BoundedQueue(std::size_t capacity)
        : capacity_{capacity} {
            if (capacity == 0) {
                throw std::invalid_argument{"BoundedQueue capacity must be at least 1"};
            }
        }
    
    // Adds an item at the back. Returns false if the queue is full or closed,
    // in which case the item is not accepted.
    bool push(T item) {
        std::unique_lock lock(mtx_);
        cvPush_.wait(lock, [this](){
            return data_.size() < capacity_ || closed_;
        });

        if(closed_) {
            return false;
        }
        data_.push(std::move(item));
        cvPop_.notify_one();
        return true;
    }

    // Blocks until an item is available or the queue is closed.
    // Returns nothing only when the queue is closed and empty.
    std::optional<T> pop() {
        std::unique_lock lock(mtx_);
        cvPop_.wait(lock, [this](){
            return !data_.empty() || closed_;
        });
        
        if(data_.empty()) {
            return {};
        }
        T first = std::move(data_.front());
        data_.pop();
        cvPush_.notify_one();
        return first;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cvPush_.notify_all();
        cvPop_.notify_all();
    }

    bool closed() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return data_.size();
    }

private:
    std::queue<T> data_;
    std::size_t capacity_;
    bool closed_ = false;
    mutable std::mutex mtx_;
    std::condition_variable cvPush_;
    std::condition_variable cvPop_;
};

} // namespace logproc