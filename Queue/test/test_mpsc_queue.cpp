#include <Queue/mpsc_queue.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#include <TestSupport/wait_for.hpp>
#include <gtest/gtest.h>

using OsborneX::Queue::MpscQueue;
using OsborneX::TestSupport::wait_for;

// These exercise the queue's contract (FIFO order, capacity respected, close()
// semantics) with at most one helper thread used only to demonstrate blocking
// -- genuine multi-producer contention/conservation is covered separately in
// test_mpsc_queue_stress.cpp.

TEST(MpscQueueTest, PushThenPopReturnsItemsInFifoOrder)
{
    MpscQueue<int> queue(4);
    queue.push(1);
    queue.push(2);
    queue.push(3);

    EXPECT_EQ(queue.pop_blocking(), 1);
    EXPECT_EQ(queue.pop_blocking(), 2);
    EXPECT_EQ(queue.pop_blocking(), 3);
}

TEST(MpscQueueTest, SizeReflectsPendingItems)
{
    MpscQueue<int> queue(4);
    EXPECT_EQ(queue.size(), 0u);
    queue.push(1);
    queue.push(2);
    EXPECT_EQ(queue.size(), 2u);
    queue.pop_blocking();
    EXPECT_EQ(queue.size(), 1u);
}

TEST(MpscQueueTest, CapacityRoundsZeroUpToOne)
{
    MpscQueue<int> queue(0);
    EXPECT_EQ(queue.capacity(), 1u);
}

TEST(MpscQueueTest, PushUpToCapacityNeverBlocks)
{
    MpscQueue<int> queue(3);
    queue.push(1);
    queue.push(2);
    queue.push(3);
    EXPECT_EQ(queue.size(), 3u);
}

TEST(MpscQueueTest, ClosingWithPendingItemsStillDrainsThemBeforeReturningNullopt)
{
    MpscQueue<int> queue(4);
    queue.push(1);
    queue.push(2);
    queue.close();

    EXPECT_EQ(queue.pop_blocking(), 1);
    EXPECT_EQ(queue.pop_blocking(), 2);
    EXPECT_EQ(queue.pop_blocking(), std::nullopt);
}

TEST(MpscQueueTest, PopBlockingOnEmptyClosedQueueReturnsNulloptImmediately)
{
    MpscQueue<int> queue(4);
    queue.close();
    EXPECT_EQ(queue.pop_blocking(), std::nullopt);
}

TEST(MpscQueueTest, PushAfterCloseIsANoOp)
{
    MpscQueue<int> queue(4);
    queue.close();
    queue.push(99);
    EXPECT_EQ(queue.size(), 0u);
    EXPECT_EQ(queue.pop_blocking(), std::nullopt);
}

TEST(MpscQueueTest, PopBlockingWaitsUntilAnItemIsPushedFromAnotherThread)
{
    MpscQueue<int> queue(4);

    std::thread producer([&] { queue.push(42); });

    // pop_blocking() has nothing to read yet in general -- it must block
    // until the producer thread's push() above wakes it, whenever that lands.
    const auto result = queue.pop_blocking();
    producer.join();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(MpscQueueTest, PushBlocksUntilConsumerFreesASlot)
{
    MpscQueue<int> queue(1);
    queue.push(1); // fills the only slot

    std::atomic<bool> push_returned{ false };
    std::thread producer([&] {
        queue.push(2); // must block until the pop below frees the slot
        push_returned.store(true, std::memory_order_release);
    });

    // The producer should still be blocked -- give it a moment to (not) proceed.
    EXPECT_FALSE(wait_for(
        [&] { return push_returned.load(std::memory_order_acquire); },
        std::chrono::milliseconds(50)));

    EXPECT_EQ(queue.pop_blocking(), 1);

    ASSERT_TRUE(wait_for([&] { return push_returned.load(std::memory_order_acquire); }));
    producer.join();
    EXPECT_EQ(queue.pop_blocking(), 2);
}
