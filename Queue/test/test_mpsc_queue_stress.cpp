#include <Queue/mpsc_queue.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using OsborneX::Queue::MpscQueue;

// Real multithreaded stress test. Run with a high iteration count for extra
// confidence, e.g. `QueueTest --gtest_filter=*Stress* --gtest_repeat=200`,
// mirroring the RingBuffer stress suite's convention.

TEST(MpscQueueStressTest, ManyProducersConserveTotalPushedEqualsTotalPopped)
{
    constexpr int producer_count = 8;
    constexpr std::uint64_t items_per_producer = 5000;
    constexpr std::uint64_t total_items = producer_count * items_per_producer;

    MpscQueue<std::uint64_t> queue(64);

    std::vector<std::thread> producers;
    for (int p = 0; p < producer_count; ++p)
    {
        producers.emplace_back([&, p] {
            for (std::uint64_t i = 0; i < items_per_producer; ++i)
                queue.push(static_cast<std::uint64_t>(p) * items_per_producer + i);
        });
    }

    std::atomic<std::uint64_t> popped_count{ 0 };
    std::uint64_t checksum = 0;

    std::thread consumer([&] {
        while (popped_count.load(std::memory_order_relaxed) < total_items)
        {
            if (const auto value = queue.pop_blocking())
            {
                checksum += *value;
                popped_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    for (auto& producer : producers)
        producer.join();
    consumer.join();

    ASSERT_EQ(popped_count.load(), total_items);

    std::uint64_t expected_checksum = 0;
    for (std::uint64_t i = 0; i < total_items; ++i)
        expected_checksum += i;
    EXPECT_EQ(checksum, expected_checksum);
}

TEST(MpscQueueStressTest, CloseDuringConcurrentPushesLetsConsumerDrainThenStop)
{
    constexpr int producer_count = 4;
    constexpr std::uint64_t items_per_producer = 2000;

    MpscQueue<std::uint64_t> queue(32);
    std::atomic<std::uint64_t> pushed_count{ 0 };

    std::vector<std::thread> producers;
    for (int p = 0; p < producer_count; ++p)
    {
        producers.emplace_back([&] {
            for (std::uint64_t i = 0; i < items_per_producer; ++i)
            {
                queue.push(i);
                pushed_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::uint64_t popped_count = 0;
    std::thread consumer([&] {
        while (queue.pop_blocking().has_value())
            ++popped_count;
    });

    for (auto& producer : producers)
        producer.join();

    // All producers are done pushing -- safe to close per MpscQueue's
    // documented hand-off contract (mirrors RingBuffer::reset_producer_thread).
    queue.close();
    consumer.join();

    EXPECT_EQ(popped_count, pushed_count.load());
}
