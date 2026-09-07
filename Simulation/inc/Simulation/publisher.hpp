#pragma once

#include <vector>

#include <Simulation/types.hpp>

namespace OsborneX::Simulation {

class Subscriber;

class MarketDataPublisher
{
public:
    void add_subscriber(Subscriber& subscriber);
    void publish(TopOfBookUpdate update);

private:
    std::vector<Subscriber*> subscribers_;
};

} // namespace OsborneX::Simulation
