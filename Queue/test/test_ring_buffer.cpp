#include <Queue/ring_buffer.hpp>

#include <gtest/gtest.h>

using OsborneX::Queue::ConsumerPolicy;
using OsborneX::Queue::ReadResult;
using OsborneX::Queue::RingBuffer;

// These tests drive the RingBuffer's producer/consumer API directly from a
// single test thread, in a fully controlled call order. That's enough to
// exercise the sequence/gating/catch-up math exhaustively without ever
// needing a real second thread -- genuine multithreaded behavior (actual
// blocking, actual concurrent torn writes) is covered separately in
// test_ring_buffer_stress.cpp.

TEST(RingBufferTest, CapacityRoundsUpToNextPowerOfTwo)
{
    EXPECT_EQ(RingBuffer<int>(1).capacity(), 1u);
    EXPECT_EQ(RingBuffer<int>(3).capacity(), 4u);
    EXPECT_EQ(RingBuffer<int>(4).capacity(), 4u);
    EXPECT_EQ(RingBuffer<int>(5).capacity(), 8u);
    EXPECT_EQ(RingBuffer<int>(0).capacity(), 1u);
}

TEST(RingBufferTest, LosslessGatingAllowsExactlyCapacityInFlight)
{
    RingBuffer<int> buffer(4);
    auto& consumer = buffer.add_consumer(ConsumerPolicy::Lossless);

    // Fill to capacity without ever calling the (potentially spinning)
    // blocking claim -- push_overwrite never blocks, so this is safe to do
    // from a single thread even though a Lossless consumer hasn't read yet.
    for (int i = 0; i < 4; ++i)
        buffer.push_overwrite(i);

    // A push_blocking() call here would spin forever: all 4 slots are
    // unread by the only Lossless consumer. Advance the consumer by exactly
    // one item first (simulating what a consumer thread would do), which
    // frees a slot -- only then is it safe to call the blocking claim from
    // this single thread, proving the gating boundary is exactly right.
    int value = -1;
    ASSERT_EQ(consumer.try_read(value), ReadResult::Ok);
    EXPECT_EQ(value, 0);

    buffer.push_blocking(99);

    for (int expected : { 1, 2, 3, 99 })
    {
        ASSERT_EQ(consumer.try_read(value), ReadResult::Ok);
        EXPECT_EQ(value, expected);
    }
    EXPECT_EQ(consumer.try_read(value), ReadResult::Empty);
    EXPECT_EQ(consumer.dropped_count(), 0u);
}

TEST(RingBufferTest, LosslessWrapsCorrectlyAcrossMultipleLaps)
{
    RingBuffer<int> buffer(4);
    auto& consumer = buffer.add_consumer(ConsumerPolicy::Lossless);

    int next_expected = 0;
    for (int lap = 0; lap < 6; ++lap)
    {
        // Keep at most 3 (< capacity) in flight so push_blocking never spins.
        for (int i = 0; i < 3; ++i)
            buffer.push_blocking(lap * 3 + i);

        for (int i = 0; i < 3; ++i)
        {
            int value = -1;
            ASSERT_EQ(consumer.try_read(value), ReadResult::Ok);
            EXPECT_EQ(value, next_expected++);
        }
    }
    EXPECT_EQ(consumer.dropped_count(), 0u);
    EXPECT_EQ(consumer.position(), buffer.produced_count());
}

TEST(RingBufferTest, LossyConsumerCatchesUpAndCountsSkippedItems)
{
    RingBuffer<int> buffer(4);
    auto& consumer = buffer.add_consumer(ConsumerPolicy::Lossy);

    for (int i = 0; i < 10; ++i)
        buffer.push_overwrite(i); // never blocks, even though nobody has read yet

    int value = -1;
    ASSERT_EQ(consumer.try_read(value), ReadResult::Ok);
    // produced=10, capacity=4 => oldest still-valid item is 10-4=6.
    EXPECT_EQ(value, 6);
    EXPECT_EQ(consumer.dropped_count(), 6u);

    for (int expected : { 7, 8, 9 })
    {
        ASSERT_EQ(consumer.try_read(value), ReadResult::Ok);
        EXPECT_EQ(value, expected);
    }
    EXPECT_EQ(consumer.try_read(value), ReadResult::Empty);
    EXPECT_EQ(consumer.dropped_count(), 6u);
}

TEST(RingBufferTest, LossAndDeliveryAlwaysConserveTotalProduced)
{
    RingBuffer<int> buffer(8);
    auto& consumer = buffer.add_consumer(ConsumerPolicy::Lossy);

    for (int i = 0; i < 37; ++i) // not a multiple of capacity, on purpose
        buffer.push_overwrite(i);

    std::uint64_t read_count = 0;
    int value = -1;
    while (consumer.try_read(value) == ReadResult::Ok)
        ++read_count;

    EXPECT_EQ(read_count + consumer.dropped_count(), buffer.produced_count());
}

namespace {

// A payload whose copy-assignment can, as a side effect, push a new item
// into the very same RingBuffer it came from. Used below to deterministically
// simulate "the producer overwrote this slot mid-copy" on a single thread:
// since everything here runs synchronously, the side effect completes before
// Consumer::try_read's post-copy re-check runs, exactly like a real producer
// racing ahead of a slow consumer would.
struct SelfMutatingPayload
{
    int value{ 0 };
    RingBuffer<SelfMutatingPayload>* buffer{ nullptr };
    int overwrite_value{ 0 };
    bool trigger_on_copy{ false };
    // Propagated across assignments (see below) to count how many times
    // *this logical item* has been assigned, not how many times any one
    // destination object has been written to.
    int assignment_count{ 0 };

    SelfMutatingPayload() = default;
    SelfMutatingPayload(const SelfMutatingPayload&) = default;

    SelfMutatingPayload& operator=(const SelfMutatingPayload& other)
    {
        value = other.value;
        buffer = other.buffer;
        overwrite_value = other.overwrite_value;
        trigger_on_copy = other.trigger_on_copy;
        assignment_count = other.assignment_count + 1;

        // Fire only on the *second* assignment of this item's lineage: the
        // first is the producer placing it into the slot (push_overwrite's
        // own `slot(seq) = std::move(value)`), which must NOT recurse here.
        // The second is Consumer::try_read's `out = slot.value` copying it
        // back out -- exactly the point we want to simulate a producer
        // racing ahead and overwriting the slot mid-copy.
        if (trigger_on_copy && assignment_count >= 2 && buffer != nullptr)
        {
            SelfMutatingPayload replacement{};
            replacement.value = overwrite_value;
            replacement.buffer = buffer;
            buffer->push_overwrite(replacement);
        }
        return *this;
    }
};

} // namespace

TEST(RingBufferTest, LossyConsumerDetectsTornWriteDuringCopyInsteadOfDeliveringIt)
{
    RingBuffer<SelfMutatingPayload> buffer(4);
    auto& consumer = buffer.add_consumer(ConsumerPolicy::Lossy);

    SelfMutatingPayload triggering{};
    triggering.value = 100;
    triggering.buffer = &buffer;
    triggering.overwrite_value = 999;
    triggering.trigger_on_copy = true;
    buffer.push_overwrite(triggering); // seq 0, slot index 0

    for (int i = 0; i < 3; ++i)
        buffer.push_overwrite(SelfMutatingPayload{}); // seq 1,2,3 (filler)

    // producer_sequence_ is now 4, matching capacity: the next push would
    // land back on slot index 0 -- exactly the slot we're about to read.
    SelfMutatingPayload out{};
    const ReadResult result = consumer.try_read(out);

    EXPECT_EQ(result, ReadResult::Dropped);
    EXPECT_EQ(consumer.dropped_count(), 1u);
    EXPECT_EQ(consumer.position(), 1u);
}
