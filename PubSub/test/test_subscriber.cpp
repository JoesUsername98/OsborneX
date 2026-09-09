#include <gtest/gtest.h>

#include <PubSub/subscriber.hpp>
#include <Queue/ring_buffer.hpp>
#include <Messages/types.hpp>
#include <TestSupport/wait_for.hpp>

#include <atomic>

namespace OsborneX::Simulation {
namespace {

using OsborneX::TestSupport::wait_for;

TopOfBookUpdate MakeUpdate(SymbolId symbol, IngressSequence sequence)
{
    return TopOfBookUpdate{
        .symbol = symbol,
        .bid_price = 100.0,
        .bid_quantity = 10,
        .ask_price = 101.0,
        .ask_quantity = 5,
        .sequence = sequence,
    };
}

TEST(SubscriberTest, DeliversUpdatesPublishedByARegisteredProducer)
{
    Queue::RingBuffer<TopOfBookUpdate> producer(16);
    Subscriber subscriber;
    subscriber.register_producer(producer);
    subscriber.start();

    producer.push_overwrite(MakeUpdate(1, 1));
    producer.push_overwrite(MakeUpdate(1, 2));

    ASSERT_TRUE(wait_for([&] { return subscriber.snapshot_events().size() == 2; }));
    subscriber.stop();

    const auto events = subscriber.snapshot_events();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].sequence, 1u);
    EXPECT_EQ(events[1].sequence, 2u);
    EXPECT_EQ(subscriber.dropped_count(), 0u);
}

TEST(SubscriberTest, InvokesHandlerForEachDeliveredUpdate)
{
    Queue::RingBuffer<TopOfBookUpdate> producer(16);
    std::atomic<int> handled{ 0 };
    Subscriber subscriber{ [&](const TopOfBookUpdate&) { handled.fetch_add(1, std::memory_order_relaxed); } };
    subscriber.register_producer(producer);
    subscriber.start();

    for (int i = 0; i < 5; ++i)
        producer.push_overwrite(MakeUpdate(2, static_cast<IngressSequence>(i)));

    ASSERT_TRUE(wait_for([&] { return handled.load(std::memory_order_relaxed) == 5; }));
    subscriber.stop();
}

TEST(SubscriberTest, MergesUpdatesFromMultipleRegisteredProducers)
{
    Queue::RingBuffer<TopOfBookUpdate> producer_a(16);
    Queue::RingBuffer<TopOfBookUpdate> producer_b(16);
    Subscriber subscriber;
    subscriber.register_producer(producer_a);
    subscriber.register_producer(producer_b);
    subscriber.start();

    producer_a.push_overwrite(MakeUpdate(1, 1));
    producer_b.push_overwrite(MakeUpdate(2, 1));
    producer_a.push_overwrite(MakeUpdate(1, 2));

    ASSERT_TRUE(wait_for([&] { return subscriber.snapshot_events().size() == 3; }));
    subscriber.stop();

    EXPECT_EQ(subscriber.dropped_count(), 0u);
}

TEST(SubscriberTest, CountsDropsFromAProducerThatOutranTheBuffer)
{
    Queue::RingBuffer<TopOfBookUpdate> producer(4); // small: easy to overrun
    Subscriber subscriber;
    subscriber.register_producer(producer);

    // Push far more than the buffer holds *before* the subscriber ever starts
    // reading, so the drop math is exercised deterministically rather than
    // racing a live producer thread.
    for (int i = 0; i < 20; ++i)
        producer.push_overwrite(MakeUpdate(4, static_cast<IngressSequence>(i)));

    subscriber.start();
    ASSERT_TRUE(wait_for([&] { return subscriber.snapshot_events().size() == 4; }));
    subscriber.stop();

    EXPECT_EQ(subscriber.dropped_count(), 16u);
    const auto events = subscriber.snapshot_events();
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events.front().sequence, 16u); // oldest surviving item after the catch-up jump
    EXPECT_EQ(events.back().sequence, 19u);
}

} // namespace
} // namespace OsborneX::Simulation
