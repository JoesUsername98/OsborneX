#include <Sharding/router.hpp>
#include <Sharding/shard.hpp>

#include <functional>

namespace OsborneX::Simulation {

Router::Router(std::vector<Shard*>& shards)
    : shards_(shards)
{
}

std::size_t Router::shard_for_symbol(SymbolId symbol, std::size_t shard_count)
{
    return std::hash<SymbolId>{}(symbol) % shard_count;
}

void Router::route(OrderMessage message)
{
    const auto shard_id = shard_for_symbol(message.symbol, shards_.size());
    shards_[shard_id]->enqueue(std::move(message));
}

} // namespace OsborneX::Simulation
