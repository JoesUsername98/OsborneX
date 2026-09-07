#include <gtest/gtest.h>

#include <Simulation/router.hpp>
#include <Simulation/types.hpp>

#include <set>

namespace OsborneX::Simulation {
namespace {

TEST(RouterTest, SameSymbolAlwaysMapsToSameShard)
{
    constexpr std::size_t shard_count = 8;
    const SymbolId symbol = 42;

    const auto first = Router::shard_for_symbol(symbol, shard_count);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(Router::shard_for_symbol(symbol, shard_count), first);

    EXPECT_LT(first, shard_count);
}

TEST(RouterTest, DifferentSymbolsCanMapToDifferentShards)
{
    constexpr std::size_t shard_count = 8;
    std::set<std::size_t> shards;

    for (SymbolId symbol = 0; symbol < 256; ++symbol)
        shards.insert(Router::shard_for_symbol(symbol, shard_count));

    EXPECT_GT(shards.size(), 1u);
    EXPECT_LE(shards.size(), shard_count);
}

} // namespace
} // namespace OsborneX::Simulation
