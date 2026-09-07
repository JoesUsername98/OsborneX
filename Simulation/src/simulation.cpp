#include <Simulation/simulation.hpp>

#include <stdexcept>
#include <utility>

namespace OsborneX::Simulation {

Simulation::Simulation(std::size_t shard_count)
    : router_(shard_ptrs_)
    , ingress_(router_)
{
    if (shard_count == 0)
        throw std::invalid_argument("shard_count must be greater than zero");

    shards_.reserve(shard_count);
    shard_ptrs_.reserve(shard_count);

    for (std::size_t i = 0; i < shard_count; ++i)
    {
        shards_.push_back(std::make_unique<Shard>(publisher_));
        shard_ptrs_.push_back(shards_.back().get());
    }
}

void Simulation::add_subscriber(Subscriber& subscriber)
{
    publisher_.add_subscriber(subscriber);
}

void Simulation::start()
{
    if (running_)
        return;

    running_ = true;
    for (auto& shard : shards_)
        shard->start();
}

void Simulation::stop()
{
    if (!running_)
        return;

    running_ = false;
    for (auto& shard : shards_)
        shard->stop();
}

void Simulation::submit(OrderMessage message)
{
    ingress_.receive(std::move(message));
}

Ingress& Simulation::ingress()
{
    return ingress_;
}

Router& Simulation::router()
{
    return router_;
}

MarketDataPublisher& Simulation::publisher()
{
    return publisher_;
}

Shard& Simulation::shard(std::size_t index)
{
    return *shards_.at(index);
}

std::size_t Simulation::shard_count() const
{
    return shards_.size();
}

} // namespace OsborneX::Simulation
