#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Thread-Safe Queue
// ═══════════════════════════════════════════════════════════════════════════════
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace ps5dm {

/// Thread-safe bounded queue with blocking operations
template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t maxSize = 0) : maxSize_(maxSize) {}

    /// Push item, blocks if queue is full (when bounded)
    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (maxSize_ > 0) {
            notFull_.wait(lock, [this] { return queue_.size() < maxSize_ || closed_; });
        }
        if (closed_) return;
        queue_.push(std::move(item));
        notEmpty_.notify_one();
    }

    /// Try push without blocking
    bool tryPush(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) return false;
        if (maxSize_ > 0 && queue_.size() >= maxSize_) return false;
        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    /// Pop item, blocks until available
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [this] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        if (maxSize_ > 0) notFull_.notify_one();
        return item;
    }

    /// Pop with timeout
    std::optional<T> popFor(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!notEmpty_.wait_for(lock, timeout, [this] { return !queue_.empty() || closed_; })) {
            return std::nullopt;
        }
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        if (maxSize_ > 0) notFull_.notify_one();
        return item;
    }

    /// Try pop without blocking
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        if (maxSize_ > 0) notFull_.notify_one();
        return item;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /// Close queue - all waiting threads will be unblocked
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
        if (maxSize_ > 0) notFull_.notify_all();
    }

    bool isClosed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    size_t maxSize_;
    bool closed_ = false;
};

} // namespace ps5dm
