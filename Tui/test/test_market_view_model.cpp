#include <Tui/market_view_model.hpp>

#include <gtest/gtest.h>

using namespace OsborneX;

namespace {

Simulation::TopOfBookUpdate MakeTop(Simulation::SymbolId symbol, Simulation::Price bid, Simulation::Price ask)
{
    return Simulation::TopOfBookUpdate{
        .symbol = symbol,
        .bid_price = bid,
        .bid_quantity = 10,
        .ask_price = ask,
        .ask_quantity = 10,
    };
}

Simulation::TradeExecution MakeTrade(Simulation::SymbolId symbol, Simulation::OrderId order_id)
{
    return Simulation::TradeExecution{
        .symbol = symbol,
        .bid_order_id = order_id,
        .bid_price = 100.0,
        .ask_order_id = order_id + 1,
        .ask_price = 100.0,
        .quantity = 5,
    };
}

} // namespace

TEST(MarketViewModelTest, StartsWithNoBooksOrTrades)
{
    Tui::MarketViewModel model;
    EXPECT_TRUE(model.snapshot_books().empty());
    EXPECT_TRUE(model.snapshot_trades().empty());
}

TEST(MarketViewModelTest, TracksLatestTopOfBookPerSymbol)
{
    Tui::MarketViewModel model;
    model.on_top_of_book(MakeTop(1, 100.0, 101.0));
    model.on_top_of_book(MakeTop(2, 50.0, 51.0));
    model.on_top_of_book(MakeTop(1, 105.0, 106.0)); // supersedes the first update for symbol 1

    const auto books = model.snapshot_books();
    ASSERT_EQ(books.size(), 2u);
    EXPECT_EQ(books[0].symbol, 1u);
    EXPECT_DOUBLE_EQ(books[0].bid_price, 105.0);
    EXPECT_EQ(books[1].symbol, 2u);
    EXPECT_DOUBLE_EQ(books[1].bid_price, 50.0);
}

TEST(MarketViewModelTest, TradeTapeOrdersMostRecentFirst)
{
    Tui::MarketViewModel model;
    model.on_trade(MakeTrade(1, 1));
    model.on_trade(MakeTrade(1, 3));
    model.on_trade(MakeTrade(1, 5));

    const auto trades = model.snapshot_trades();
    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].bid_order_id, 5u);
    EXPECT_EQ(trades[1].bid_order_id, 3u);
    EXPECT_EQ(trades[2].bid_order_id, 1u);
}

TEST(MarketViewModelTest, TradeTapeIsBoundedToConfiguredSize)
{
    Tui::MarketViewModel model(3);
    for (Simulation::OrderId i = 0; i < 10; i += 2)
        model.on_trade(MakeTrade(1, i));

    const auto trades = model.snapshot_trades();
    ASSERT_EQ(trades.size(), 3u);
    // Most recent three: order ids 8, 6, 4 (bid ids are i itself).
    EXPECT_EQ(trades[0].bid_order_id, 8u);
    EXPECT_EQ(trades[1].bid_order_id, 6u);
    EXPECT_EQ(trades[2].bid_order_id, 4u);
}

TEST(MarketViewModelTest, ZeroCapacityIsRoundedUpToOne)
{
    Tui::MarketViewModel model(0);
    model.on_trade(MakeTrade(1, 1));
    model.on_trade(MakeTrade(1, 2));

    const auto trades = model.snapshot_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].bid_order_id, 2u);
}
