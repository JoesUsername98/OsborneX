#include <gtest/gtest.h>

#include <Simulation/publisher.hpp>
#include <Simulation/shard.hpp>
#include <Simulation/subscriber.hpp>
#include <Simulation/types.hpp>

#include <chrono>
#include <thread>

namespace OsborneX::Simulation {
namespace {

OrderMessage MakeAdd(
    SymbolId symbol,
    OrderId orderId,
    Side side,
    Price price,
    Quantity quantity,
    IngressSequence sequence = 1)
{
    return OrderMessage{
        .source_timestamp = 0,
        .ingress_sequence = sequence,
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

OrderMessage MakeCancel(SymbolId symbol, OrderId orderId, IngressSequence sequence)
{
    return OrderMessage{
        .ingress_sequence = sequence,
        .source = 1,
        .symbol = symbol,
        .order_id = orderId,
        .action = OrderAction::Cancel,
    };
}

TEST(ShardTest, RestingAddPublishesTopOfBook)
{
    MarketDataPublisher publisher;
    Subscriber subscriber;
    publisher.add_subscriber(subscriber);
    subscriber.start();

    Shard shard{ publisher };
    shard.start();
    shard.enqueue(MakeAdd(7, 1, Side::Buy, 100.0, 10, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    shard.stop();
    subscriber.stop();

    const auto events = subscriber.snapshot_events();
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back().symbol, 7u);
    EXPECT_EQ(events.back().bid_quantity, 10u);
    EXPECT_DOUBLE_EQ(events.back().bid_price, 100.0);
    EXPECT_EQ(shard.book_size(7), 1u);
}

TEST(ShardTest, CrossingAddClearsMatchedLiquidityFromTop)
{
    MarketDataPublisher publisher;
    Subscriber subscriber;
    publisher.add_subscriber(subscriber);
    subscriber.start();

    Shard shard{ publisher };
    shard.start();
    shard.enqueue(MakeAdd(1, 1, Side::Buy, 100.0, 10, 1));
    shard.enqueue(MakeAdd(1, 2, Side::Sell, 100.0, 10, 2));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    shard.stop();
    subscriber.stop();

    EXPECT_EQ(shard.book_size(1), 0u);
    const auto tob = shard.top_of_book(1);
    EXPECT_EQ(tob.bid_quantity, 0u);
    EXPECT_EQ(tob.ask_quantity, 0u);
}

TEST(ShardTest, CancelRemovesRestingOrder)
{
    MarketDataPublisher publisher;
    Subscriber subscriber;
    publisher.add_subscriber(subscriber);
    subscriber.start();

    Shard shard{ publisher };
    shard.start();
    shard.enqueue(MakeAdd(3, 9, Side::Sell, 55.0, 4, 1));
    shard.enqueue(MakeCancel(3, 9, 2));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    shard.stop();
    subscriber.stop();

    EXPECT_EQ(shard.book_size(3), 0u);
    const auto events = subscriber.snapshot_events();
    ASSERT_GE(events.size(), 2u);
    EXPECT_EQ(events.back().ask_quantity, 0u);
}

} // namespace
} // namespace OsborneX::Simulation
