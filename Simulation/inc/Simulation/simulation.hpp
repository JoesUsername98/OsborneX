#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <Simulation/ingress.hpp>
#include <Simulation/publisher.hpp>
#include <Simulation/router.hpp>
#include <Simulation/shard.hpp>
#include <Simulation/subscriber.hpp>
#include <Simulation/types.hpp>

namespace OsborneX::Simulation {

class Simulation
{
public:
    explicit Simulation(std::size_t shard_count = 4);

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    void add_subscriber(Subscriber& subscriber);
    void start();
    void stop();

    void submit(OrderMessage message);
    Ingress& ingress();
    Router& router();
    MarketDataPublisher& publisher();
    Shard& shard(std::size_t index);
    std::size_t shard_count() const;

private:
    MarketDataPublisher publisher_;
    std::vector<std::unique_ptr<Shard>> shards_;
    std::vector<Shard*> shard_ptrs_;
    Router router_;
    Ingress ingress_;
    bool running_{ false };
};

} // namespace OsborneX::Simulation
