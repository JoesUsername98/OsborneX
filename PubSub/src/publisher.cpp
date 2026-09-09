#include <PubSub/publisher.hpp>
#include <PubSub/subscriber.hpp>

#include <cassert>

namespace OsborneX::Simulation {

void MarketDataPublisher::add_subscriber(Subscriber& subscriber)
{
    assert(!frozen_ && "MarketDataPublisher: add_subscriber() called after freeze()");
    subscribers_.push_back(&subscriber);
    for (Queue::RingBuffer<TopOfBookUpdate>* producer : producers_)
        subscriber.register_producer(*producer);
}

void MarketDataPublisher::add_producer(Queue::RingBuffer<TopOfBookUpdate>& producer)
{
    assert(!frozen_ && "MarketDataPublisher: add_producer() called after freeze()");
    producers_.push_back(&producer);
    for (Subscriber* subscriber : subscribers_)
        subscriber->register_producer(producer);
}

void MarketDataPublisher::freeze()
{
    frozen_ = true;
}

} // namespace OsborneX::Simulation
