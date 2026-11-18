#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace orderflow {

// A blocking FIFO queue that is safe to use from many producer and consumer
// threads. Consumers block in pop() until an item arrives or the queue is
// closed. After close(), remaining items can still be drained, and pop()
// returns std::nullopt once the queue is empty.
template <typename T>
class ThreadSafeQueue {
public:
    // Returns false if the queue has been closed and the item was rejected.
    bool push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) {
                return false;
            }
            queue_.push(std::move(item));
        }
        cv_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;  // closed and fully drained
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool closed_ = false;
};

}  // namespace orderflow
