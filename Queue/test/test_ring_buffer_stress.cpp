#include <Queue/ring_buffer.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using OsborneX::Queue::ConsumerPolicy;
using OsborneX::Queue::ReadResult;
using OsborneX::Queue::RingBuffer;

// Real multithreaded stress tests. Run these many times / with a high
// iteration count (e.g. `QueueTest --gtest_filter=*Stress* --gtest_repeat=200`)
// to build confidence beyond what a single run can show -- this is the
// brute-force half of the "test multithreaded code" playbook: many
// producer/consumer threads, many iterations, hard conservation invariants
// checked every run, rather than relying on a sanitizer to catch a race.

TEST(RingBufferStressTest, LosslessSingleProducerSingleConsumerDeliversEveryItemExactlyOnceInOrder)
{
    constexpr std::uint64_t item_count = 200000;
    RingBuffer<std::uint64_t> buffer(64);
    auto& consumer = buffer.add_consumer(ConsumerPolicy::Lossless);

    std::vector<std::uint64_t> received;
    received.reserve(item_count);

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < item_count; ++i)
            buffer.push_blocking(i);
    });

    std::thread consumer_thread([&] {
        std::uint64_t value = 0;
        while (received.size() < item_count)
        {
            const ReadResult result = consumer.try_read(value);
            if (result == ReadResult::Ok)
            {
                received.push_back(value);
            }
            else if (result == ReadResult::Empty)
            {
                std::this_thread::yield();
            }
            else
            {
                ADD_FAILURE() << "Lossless consumer must never report Dropped";
                return;
            }
        }
    });

    producer.join();
    consumer_thread.join();

    ASSERT_EQ(received.size(), item_count);
    for (std::uint64_t i = 0; i < item_count; ++i)
        ASSERT_EQ(received[i], i) << "mismatch at index " << i;
    EXPECT_EQ(consumer.dropped_count(), 0u);
    EXPECT_EQ(consumer.position(), buffer.produced_count());
}

TEST(RingBufferStressTest, LossyConsumersConserveReadPlusDroppedEqualsProduced)
{
    constexpr std::uint64_t item_count = 50000;
    // Deliberately small so the throttled consumer genuinely falls behind
    // and has to skip forward, rather than merely testing the happy path.
    RingBuffer<std::uint64_t> buffer(32);
    auto& fast_consumer = buffer.add_consumer(ConsumerPolicy::Lossy);
    auto& slow_consumer = buffer.add_consumer(ConsumerPolicy::Lossy);

    std::atomic<std::uint64_t> fast_read{ 0 };
    std::atomic<std::uint64_t> slow_read{ 0 };
    std::atomic<bool> producer_done{ false };

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < item_count; ++i)
            buffer.push_overwrite(i);
        producer_done.store(true, std::memory_order_release);
    });

    auto drain = [&](RingBuffer<std::uint64_t>::Consumer& consumer,
                      std::atomic<std::uint64_t>& read_count,
                      bool throttle) {
        std::uint64_t value = 0;
        while (true)
        {
            const ReadResult result = consumer.try_read(value);
            if (result == ReadResult::Ok)
            {
                read_count.fetch_add(1, std::memory_order_relaxed);
                if (throttle)
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
            else if (result == ReadResult::Dropped)
            {
                // still progress; nothing to record beyond the consumer's own dropped_count()
            }
            else // Empty
            {
                if (producer_done.load(std::memory_order_acquire) &&
                    consumer.position() >= buffer.produced_count())
                    break;
                std::this_thread::yield();
            }
        }
    };

    std::thread fast_thread([&] { drain(fast_consumer, fast_read, false); });
    std::thread slow_thread([&] { drain(slow_consumer, slow_read, true); });

    producer.join();
    fast_thread.join();
    slow_thread.join();

    const std::uint64_t produced = buffer.produced_count();
    ASSERT_EQ(produced, item_count);
    EXPECT_EQ(fast_read.load() + fast_consumer.dropped_count(), produced);
    EXPECT_EQ(slow_read.load() + slow_consumer.dropped_count(), produced);
    EXPECT_GT(slow_consumer.dropped_count(), 0u)
        << "throttled consumer should have fallen behind and dropped at least one item";
}

TEST(RingBufferStressTest, ManyLosslessConsumersEachReceiveTheFullStreamIndependently)
{
    constexpr std::uint64_t item_count = 20000;
    constexpr int consumer_count = 4;
    RingBuffer<std::uint64_t> buffer(64);

    std::vector<RingBuffer<std::uint64_t>::Consumer*> consumers;
    for (int i = 0; i < consumer_count; ++i)
        consumers.push_back(&buffer.add_consumer(ConsumerPolicy::Lossless));

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < item_count; ++i)
            buffer.push_blocking(i);
    });

    std::vector<std::thread> consumer_threads;
    std::vector<std::uint64_t> received_counts(consumer_count, 0);
    std::vector<char> mismatched(consumer_count, 0);

    for (int i = 0; i < consumer_count; ++i)
    {
        consumer_threads.emplace_back([&, i] {
            std::uint64_t expected = 0;
            std::uint64_t value = 0;
            while (received_counts[i] < item_count)
            {
                const ReadResult result = consumers[i]->try_read(value);
                if (result == ReadResult::Ok)
                {
                    if (value != expected)
                        mismatched[i] = 1;
                    ++expected;
                    ++received_counts[i];
                }
                else if (result == ReadResult::Empty)
                {
                    std::this_thread::yield();
                }
                else
                {
                    mismatched[i] = 1;
                    return;
                }
            }
        });
    }

    producer.join();
    for (auto& t : consumer_threads)
        t.join();

    for (int i = 0; i < consumer_count; ++i)
    {
        EXPECT_EQ(received_counts[i], item_count) << "consumer " << i;
        EXPECT_FALSE(mismatched[i]) << "consumer " << i << " saw an out-of-order or dropped item";
        EXPECT_EQ(consumers[i]->dropped_count(), 0u) << "consumer " << i;
    }
}
