#pragma once

#include <cstddef>
#include <vector>

#include <Messages/order_sink.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Simulation {

class Shard;

class Router : public OrderSink
{
public:
    explicit Router(std::vector<Shard*>& shards);

    void route(OrderMessage message) override;
    static std::size_t shard_for_symbol(SymbolId symbol, std::size_t shard_count);

private:
    std::vector<Shard*>& shards_;
};

} // namespace OsborneX::Simulation
