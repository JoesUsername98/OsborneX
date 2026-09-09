#include <gtest/gtest.h>

#include <PubSub/publisher.hpp>
#include <PubSub/subscriber.hpp>
#include <Queue/ring_buffer.hpp>
#include <Messages/types.hpp>
#include <TestSupport/wait_for.hpp>

namespace OsborneX::Simulation {
namespace {

using OsborneX::TestSupport::wait_for;

TopOfBookUpdate MakeUpdate(SymbolId symbol, IngressSequence sequence)
{
    return TopOfBookUpdate{ .symbol = symbol, .sequence = sequence };
}

TEST(MarketDataPublisherTest, WiresSubscriberAddedBeforeProducer)
{
    MarketDataPublisher publisher;
    Subscriber subscriber;
    Queue::RingBuffer<TopOfBookUpdate> producer(16);

    publisher.add_subscriber(subscriber);
    publisher.add_producer(producer);
    publisher.freeze();

    subscriber.start();
    producer.push_overwrite(MakeUpdate(1, 1));

    ASSERT_TRUE(wait_for([&] { return subscriber.snapshot_events().size() == 1; }));
    subscriber.stop();
}

TEST(MarketDataPublisherTest, WiresProducerAddedBeforeSubscriber)
{
    MarketDataPublisher publisher;
    Queue::RingBuffer<TopOfBookUpdate> producer(16);
    Subscriber subscriber;

    publisher.add_producer(producer);
    publisher.add_subscriber(subscriber);
    publisher.freeze();

    subscriber.start();
    producer.push_overwrite(MakeUpdate(2, 1));

    ASSERT_TRUE(wait_for([&] { return subscriber.snapshot_events().size() == 1; }));
    subscriber.stop();
}

TEST(MarketDataPublisherTest, WiresMultipleSubscribersToASingleProducer)
{
    MarketDataPublisher publisher;
    Queue::RingBuffer<TopOfBookUpdate> producer(16);
    Subscriber subscriber_a;
    Subscriber subscriber_b;

    publisher.add_producer(producer);
    publisher.add_subscriber(subscriber_a);
    publisher.add_subscriber(subscriber_b);
    publisher.freeze();

    subscriber_a.start();
    subscriber_b.start();
    producer.push_overwrite(MakeUpdate(3, 1));

    ASSERT_TRUE(wait_for([&] { return subscriber_a.snapshot_events().size() == 1; }));
    ASSERT_TRUE(wait_for([&] { return subscriber_b.snapshot_events().size() == 1; }));
    subscriber_a.stop();
    subscriber_b.stop();
}

#ifndef NDEBUG
TEST(MarketDataPublisherDeathTest, AddSubscriberAfterFreezeAsserts)
{
    MarketDataPublisher publisher;
    publisher.freeze();

    Subscriber subscriber;
    EXPECT_DEATH(publisher.add_subscriber(subscriber), "");
}

TEST(MarketDataPublisherDeathTest, AddProducerAfterFreezeAsserts)
{
    MarketDataPublisher publisher;
    publisher.freeze();

    Queue::RingBuffer<TopOfBookUpdate> producer(4);
    EXPECT_DEATH(publisher.add_producer(producer), "");
}
#endif

} // namespace
} // namespace OsborneX::Simulation
