#include <gtest/gtest.h>

#include <Simulation/simulation.hpp>
#include <Simulation/types.hpp>

#include <chrono>
#include <thread>
#include <vector>

namespace OsborneX::Simulation {
namespace {

OrderMessage MakeAdd(
    SymbolId symbol,
    OrderId orderId,
    Side side,
    Price price,
    Quantity quantity)
{
    return OrderMessage{
        .source = 1,
        .symbol = symbol,
        .order_id = orderId,
        .side = side,
        .price = price,
        .quantity = quantity,
        .type = OrderType::GoodTillCancel,
        .action = OrderAction::Add,
    };
}

TEST(ConcurrencyTest, ManySymbolsAcrossShardsCompleteWithoutCrash)
{
    constexpr std::size_t shard_count = 4;
    constexpr SymbolId symbol_count = 64;

    Simulation simulation{ shard_count };
    Subscriber subscriber{ 4096 };
    simulation.add_subscriber(subscriber);

    subscriber.start();
    simulation.start();

    OrderId next_id = 1;
    for (SymbolId symbol = 0; symbol < symbol_count; ++symbol)
    {
        simulation.submit(MakeAdd(symbol, next_id++, Side::Buy, 100.0 + symbol, 10));
        simulation.submit(MakeAdd(symbol, next_id++, Side::Sell, 200.0 + symbol, 10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    simulation.stop();
    subscriber.stop();

    for (SymbolId symbol = 0; symbol < symbol_count; ++symbol)
    {
        const auto shard_index = Router::shard_for_symbol(symbol, shard_count);
        EXPECT_EQ(simulation.shard(shard_index).book_size(symbol), 2u);

        const auto tob = simulation.shard(shard_index).top_of_book(symbol);
        EXPECT_EQ(tob.bid_quantity, 10u);
        EXPECT_EQ(tob.ask_quantity, 10u);
        EXPECT_DOUBLE_EQ(tob.bid_price, 100.0 + symbol);
        EXPECT_DOUBLE_EQ(tob.ask_price, 200.0 + symbol);
    }

    EXPECT_GE(subscriber.snapshot_events().size(), symbol_count);
}

TEST(DropPolicyTest, FullSubscriberQueueDropsUpdatesWithoutBlockingShards)
{
    Simulation simulation{ 2 };
    Subscriber subscriber{ 1 };
    simulation.add_subscriber(subscriber);

    // Do not start the subscriber worker so the queue cannot drain.
    simulation.start();

    for (OrderId id = 1; id <= 200; ++id)
        simulation.submit(MakeAdd(1, id, Side::Buy, 100.0 + static_cast<double>(id), 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    simulation.stop();

    EXPECT_GT(subscriber.dropped_count(), 0u);
    EXPECT_EQ(simulation.shard(Router::shard_for_symbol(1, 2)).book_size(1), 200u);
}

} // namespace
} // namespace OsborneX::Simulation
