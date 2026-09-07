#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

#include "readerwriterqueue.h"
#include "concurrentqueue.h"

namespace OsborneX::Simulation {

/// Single-producer, single-consumer queue (Router → Shard).
template <typename T>
class SpscQueue
{
public:
    explicit SpscQueue(std::size_t capacity = 1024)
        : queue_(capacity)
    {
    }

    bool try_push(const T& item)
    {
        return queue_.try_enqueue(item);
    }

    bool try_push(T&& item)
    {
        return queue_.try_enqueue(std::move(item));
    }

    bool try_pop(T& item)
    {
        return queue_.try_dequeue(item);
    }

private:
    moodycamel::ReaderWriterQueue<T> queue_;
};

/// Multi-producer, single-consumer queue (Shards → Subscriber) with drop-on-full.
template <typename T>
class DroppingMpscQueue
{
public:
    explicit DroppingMpscQueue(std::size_t max_size = 1024)
        : max_size_(max_size)
        , queue_(static_cast<std::size_t>(max_size))
    {
    }

    /// Returns false if the update was dropped because the queue is full.
    bool try_push(T item)
    {
        if (queue_.size_approx() >= max_size_)
        {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        if (!queue_.enqueue(std::move(item)))
        {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        return true;
    }

    bool try_pop(T& item)
    {
        return queue_.try_dequeue(item);
    }

    std::size_t dropped_count() const
    {
        return dropped_.load(std::memory_order_relaxed);
    }

    std::size_t max_size() const
    {
        return max_size_;
    }

private:
    std::size_t max_size_;
    std::atomic<std::size_t> dropped_{ 0 };
    moodycamel::ConcurrentQueue<T> queue_;
};

} // namespace OsborneX::Simulation
