#include <gtest/gtest.h>

#include <Queue/ring_buffer.hpp>
#include <Sharding/shard.hpp>
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
    Shard shard;
    auto& market_data = shard.market_data_out().add_consumer(Queue::ConsumerPolicy::Lossy);
    shard.start();
    shard.enqueue(MakeAdd(7, 1, Side::Buy, 100.0, 10, 1));

    ASSERT_TRUE(wait_for([&] { return shard.book_size(7) == 1; }));
    shard.stop();

    TopOfBookUpdate update{};
    ASSERT_EQ(market_data.try_read(update), Queue::ReadResult::Ok);
    EXPECT_EQ(update.symbol, 7u);
    EXPECT_EQ(update.bid_quantity, 10u);
    EXPECT_DOUBLE_EQ(update.bid_price, 100.0);
    EXPECT_EQ(shard.book_size(7), 1u);
}

TEST(ShardTest, CrossingAddClearsMatchedLiquidityFromTop)
{
    Shard shard;
    shard.start();
    shard.enqueue(MakeAdd(1, 1, Side::Buy, 100.0, 10, 1));
    shard.enqueue(MakeAdd(1, 2, Side::Sell, 100.0, 10, 2));

    ASSERT_TRUE(wait_for([&] { return shard.book_size(1) == 0; }));
    shard.stop();

    EXPECT_EQ(shard.book_size(1), 0u);
    const auto tob = shard.top_of_book(1);
    EXPECT_EQ(tob.bid_quantity, 0u);
    EXPECT_EQ(tob.ask_quantity, 0u);
}

TEST(ShardTest, CancelRemovesRestingOrder)
{
    Shard shard;
    auto& market_data = shard.market_data_out().add_consumer(Queue::ConsumerPolicy::Lossy);
    shard.start();
    shard.enqueue(MakeAdd(3, 9, Side::Sell, 55.0, 4, 1));
    shard.enqueue(MakeCancel(3, 9, 2));

    ASSERT_TRUE(wait_for([&] { return shard.book_size(3) == 0; }));
    shard.stop();

    EXPECT_EQ(shard.book_size(3), 0u);

    TopOfBookUpdate update{};
    int delivered = 0;
    while (market_data.try_read(update) == Queue::ReadResult::Ok)
        ++delivered;

    ASSERT_GE(delivered, 2);
    EXPECT_EQ(update.ask_quantity, 0u);
}

} // namespace
} // namespace OsborneX::Simulation
