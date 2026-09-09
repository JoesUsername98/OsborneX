#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <Ingress/ingress.hpp>
#include <PubSub/publisher.hpp>
#include <PubSub/subscriber.hpp>
#include <Sharding/router.hpp>
#include <Sharding/shard.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Simulation {

struct SimulationOptions
{
    std::size_t shard_count{ 4 };
    std::size_t shard_inbound_capacity{ 4096 };
    std::size_t shard_outbound_capacity{ 1024 };
};

class Simulation
{
public:
    explicit Simulation(SimulationOptions options = {});
    explicit Simulation(std::size_t shard_count);

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
