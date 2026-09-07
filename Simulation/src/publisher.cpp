#include <Simulation/publisher.hpp>
#include <Simulation/subscriber.hpp>

namespace OsborneX::Simulation {

void MarketDataPublisher::add_subscriber(Subscriber& subscriber)
{
    subscribers_.push_back(&subscriber);
}

void MarketDataPublisher::publish(TopOfBookUpdate update)
{
    for (Subscriber* subscriber : subscribers_)
        subscriber->enqueue(update);
}

} // namespace OsborneX::Simulation
