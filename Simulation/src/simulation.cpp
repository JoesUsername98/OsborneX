#include <Simulation/simulation.hpp>

#include <stdexcept>
#include <utility>

namespace OsborneX::Simulation {

namespace {

SimulationOptions MakeOptions(std::size_t shard_count)
{
    return SimulationOptions{ .shard_count = shard_count };
}

} // namespace

Simulation::Simulation(SimulationOptions options)
    : router_(shard_ptrs_)
    , ingress_(router_)
{
    if (options.shard_count == 0)
        throw std::invalid_argument("shard_count must be greater than zero");

    shards_.reserve(options.shard_count);
    shard_ptrs_.reserve(options.shard_count);

    for (std::size_t i = 0; i < options.shard_count; ++i)
    {
        shards_.push_back(std::make_unique<Shard>(options.shard_inbound_capacity, options.shard_outbound_capacity));
        shard_ptrs_.push_back(shards_.back().get());
        publisher_.add_producer(shards_.back()->market_data_out());
    }
}

Simulation::Simulation(std::size_t shard_count)
    : Simulation(MakeOptions(shard_count))
{
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
    publisher_.freeze();
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
