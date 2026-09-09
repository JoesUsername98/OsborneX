#include <gtest/gtest.h>

#include <Simulation/simulation.hpp>
#include <Messages/types.hpp>
#include <TestSupport/wait_for.hpp>

namespace OsborneX::Simulation {
namespace {

using OsborneX::TestSupport::wait_for;

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
    Subscriber subscriber;
    simulation.add_subscriber(subscriber);

    subscriber.start();
    simulation.start();

    OrderId next_id = 1;
    for (SymbolId symbol = 0; symbol < symbol_count; ++symbol)
    {
        simulation.submit(MakeAdd(symbol, next_id++, Side::Buy, 100.0 + symbol, 10));
        simulation.submit(MakeAdd(symbol, next_id++, Side::Sell, 200.0 + symbol, 10));
    }

    ASSERT_TRUE(wait_for([&] {
        for (SymbolId symbol = 0; symbol < symbol_count; ++symbol)
        {
            const auto shard_index = Router::shard_for_symbol(symbol, shard_count);
            if (simulation.shard(shard_index).book_size(symbol) != 2)
                return false;
        }
        return true;
    }));

    ASSERT_TRUE(wait_for([&] { return subscriber.snapshot_events().size() >= symbol_count; }));

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

TEST(DropPolicyTest, SlowSubscriberDropsMarketDataWithoutBlockingShards)
{
    // Under the old push-based queue, a "drop" was counted the moment a
    // producer's push failed against a full queue -- so a never-started
    // subscriber guaranteed drops by construction. Under the pull-based
    // RingBuffer, a drop is *consumer-observed*: nothing is lost until some
    // consumer actually reads and notices it has been lapped. So instead we
    // let the shard finish publishing everything into a deliberately tiny
    // outbound buffer *before* the subscriber ever starts reading -- that
    // makes the overrun (and therefore the drop) deterministic rather than a
    // race against a live consumer thread.
    SimulationOptions options{
        .shard_count = 2,
        .shard_outbound_capacity = 4,
    };
    Simulation simulation{ options };
    Subscriber subscriber;
    simulation.add_subscriber(subscriber);

    simulation.start(); // shard threads only; subscriber.start() comes later, on purpose

    for (OrderId id = 1; id <= 200; ++id)
        simulation.submit(MakeAdd(1, id, Side::Buy, 100.0 + static_cast<double>(id), 1));

    ASSERT_TRUE(wait_for([&] {
        return simulation.shard(Router::shard_for_symbol(1, 2)).book_size(1) == 200;
    }));

    subscriber.start();
    ASSERT_TRUE(wait_for([&] { return subscriber.dropped_count() > 0; }));

    simulation.stop();
    subscriber.stop();

    EXPECT_GT(subscriber.dropped_count(), 0u);
    EXPECT_EQ(simulation.shard(Router::shard_for_symbol(1, 2)).book_size(1), 200u);
}

} // namespace
} // namespace OsborneX::Simulation
