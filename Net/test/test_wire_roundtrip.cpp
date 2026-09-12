#include <Net/wire/frame.hpp>
#include <Net/wire/market_data_wire.hpp>
#include <Net/wire/order_entry_wire.hpp>

#include <gtest/gtest.h>

using namespace OsborneX;

TEST(WireRoundtripTest, OrderEntryWireRoundtripsAllFields)
{
    Simulation::OrderMessage message{
        .source_timestamp = 123456789,
        .ingress_timestamp = 999, // not carried over the wire -- server stamps it
        .ingress_sequence = 42,   // not carried over the wire -- server stamps it
        .source = 7,
        .symbol = 99,
        .order_id = 555,
        .side = Simulation::Side::Sell,
        .price = 123.75,
        .quantity = 10,
        .type = Simulation::OrderType::FillOrKill,
        .action = Simulation::OrderAction::Modify,
    };

    const auto wire = Net::wire::encode(message);
    const auto decoded = Net::wire::decode(wire);

    EXPECT_EQ(decoded.source_timestamp, message.source_timestamp);
    EXPECT_EQ(decoded.ingress_timestamp, 0u);
    EXPECT_EQ(decoded.ingress_sequence, 0u);
    EXPECT_EQ(decoded.source, message.source);
    EXPECT_EQ(decoded.symbol, message.symbol);
    EXPECT_EQ(decoded.order_id, message.order_id);
    EXPECT_EQ(decoded.side, message.side);
    EXPECT_DOUBLE_EQ(decoded.price, message.price);
    EXPECT_EQ(decoded.quantity, message.quantity);
    EXPECT_EQ(decoded.type, message.type);
    EXPECT_EQ(decoded.action, message.action);
}

TEST(WireRoundtripTest, TopOfBookUpdateWireRoundtripsAllFields)
{
    Simulation::TopOfBookUpdate update{
        .symbol = 3,
        .bid_price = 100.5,
        .bid_quantity = 10,
        .ask_price = 101.0,
        .ask_quantity = 20,
        .sequence = 7,
    };

    const auto wire = Net::wire::encode(update);
    const auto decoded = Net::wire::decode(wire);

    EXPECT_EQ(decoded.symbol, update.symbol);
    EXPECT_DOUBLE_EQ(decoded.bid_price, update.bid_price);
    EXPECT_EQ(decoded.bid_quantity, update.bid_quantity);
    EXPECT_DOUBLE_EQ(decoded.ask_price, update.ask_price);
    EXPECT_EQ(decoded.ask_quantity, update.ask_quantity);
    EXPECT_EQ(decoded.sequence, update.sequence);
}

TEST(WireRoundtripTest, TradeExecutionWireRoundtripsAllFields)
{
    Simulation::TradeExecution trade{
        .symbol = 4,
        .sequence = 11,
        .timestamp = 22,
        .bid_order_id = 1,
        .bid_price = 50.0,
        .ask_order_id = 2,
        .ask_price = 50.5,
        .quantity = 6,
    };

    const auto wire = Net::wire::encode(trade);
    const auto decoded = Net::wire::decode(wire);

    EXPECT_EQ(decoded.symbol, trade.symbol);
    EXPECT_EQ(decoded.sequence, trade.sequence);
    EXPECT_EQ(decoded.timestamp, trade.timestamp);
    EXPECT_EQ(decoded.bid_order_id, trade.bid_order_id);
    EXPECT_DOUBLE_EQ(decoded.bid_price, trade.bid_price);
    EXPECT_EQ(decoded.ask_order_id, trade.ask_order_id);
    EXPECT_DOUBLE_EQ(decoded.ask_price, trade.ask_price);
    EXPECT_EQ(decoded.quantity, trade.quantity);
}

TEST(WireRoundtripTest, FrameHeaderHasExpectedFixedSize)
{
    EXPECT_EQ(sizeof(Net::wire::FrameHeader), 8u);
}
