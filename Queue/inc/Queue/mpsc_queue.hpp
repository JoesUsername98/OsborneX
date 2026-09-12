#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace OsborneX::Queue {

/// @brief Bounded, blocking, multi-producer/single-consumer queue.
/// @details Unlike RingBuffer (single-producer by contract), this exists specifically
///          for funneling many independent producer threads onto one consumer -- e.g.
///          several network connection threads each decoding orders, all of which must
///          eventually reach a single thread that owns downstream single-producer
///          invariants (Ingress::receive, RingBuffer's producer API). Backed by a plain
///          mutex/condition_variable pair rather than anything lock-free: producer-side
///          contention here is bounded by connection count, not a hot matching-engine
///          path, so simplicity is preferred over a lock-free MPSC design.
template <typename T>
class MpscQueue
{
public:
    explicit MpscQueue(std::size_t capacity = 4096)
        : capacity_{ capacity == 0 ? std::size_t{ 1 } : capacity }
    {
    }

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;
    MpscQueue(MpscQueue&&) = delete;
    MpscQueue& operator=(MpscQueue&&) = delete;

    /// @brief Pushes a value, blocking while the queue is full. Safe from any number of
    ///        producer threads. A push arriving after close() is a no-op (the queue is
    ///        shutting down and no consumer will ever observe it).
    void push(T value)
    {
        std::unique_lock lock{ mutex_ };
        not_full_.wait(lock, [this] { return items_.size() < capacity_ || closed_; });
        if (closed_)
            return;

        items_.push_back(std::move(value));
        lock.unlock();
        not_empty_.notify_one();
    }

    /// @brief Blocks until an item is available or the queue is closed and drained.
    /// @details Single-consumer only: concurrent callers would race over the same item.
    std::optional<T> pop_blocking()
    {
        std::unique_lock lock{ mutex_ };
        not_empty_.wait(lock, [this] { return !items_.empty() || closed_; });
        if (items_.empty())
            return std::nullopt;

        T value = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    /// @brief Wakes any blocked push()/pop_blocking() calls. Idempotent. Once closed,
    ///        pop_blocking() continues to drain whatever remains, then returns nullopt.
    /// @details Must only be called once every producer thread has stopped pushing --
    ///          same hand-off discipline as RingBuffer::reset_producer_thread(), just
    ///          enforced by caller convention rather than a debug assert here, since
    ///          this type has no single recorded "the producer thread" to check against.
    void close()
    {
        {
            std::lock_guard lock{ mutex_ };
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t size() const
    {
        std::lock_guard lock{ mutex_ };
        return items_.size();
    }

    std::size_t capacity() const noexcept { return capacity_; }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> items_;
    std::size_t capacity_;
    bool closed_{ false };
};

} // namespace OsborneX::Queue
