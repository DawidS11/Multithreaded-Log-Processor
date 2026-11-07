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
        : capacity{capacity} {
            if (capacity == 0) {
                throw std::invalid_argument{"BoundedQueue capacity must be at least 1"};
            }
        }
    
    // Adds an item at the back. Returns false if the queue is full or closed,
    // in which case the item is not accepted.
    bool push(T item) {
        std::unique_lock lock(mtx);
        cvPush.wait(lock, [this](){
            return data.size() < capacity || closed;
        });

        if(closed) {
            return false;
        }
        data.push(std::move(item));
        cvPop.notify_one();
        return true;
    }

    // Blocks until an item is available or the queue is closed.
    // Returns nothing only when the queue is closed and empty.
    std::optional<T> pop() {
        std::unique_lock lock(mtx);
        cvPop.wait(lock, [this](){
            return !data.empty() || closed;
        });
        
        if(data.empty()) {
            return {};
        }
        T first = std::move(data.front());
        data.pop();
        cvPush.notify_one();
        return first;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx);
        closed = true;
        cvPush.notify_all();
        cvPop.notify_all();
    }

    bool isClosed() const {
        std::lock_guard<std::mutex> lock(mtx);
        return closed;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.size();
    }

private:
    std::queue<T> data;
    std::size_t capacity;
    bool closed = false;
    mutable std::mutex mtx;
    std::condition_variable cvPush;
    std::condition_variable cvPop;
};

} // namespace logproc