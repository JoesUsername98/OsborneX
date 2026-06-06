#include <gtest/gtest.h>
#include <chrono>

#include <Orderbook/orderbook.hpp>

namespace OsborneX {
namespace {

TEST(OrderbookTestModify, IfUnknownOrderModified_WhenEmpty_ThenReturnsEmpty) {
    Orderbook orderbook{ std::chrono::hours{16} };

    const auto trades = orderbook.ModifyOrder(
        OrderModify{ OrderId{99}, Side::Buy, Price{6.7}, Quantity{1} });

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderbook.Size(), std::size_t{0});
}

TEST(OrderbookTestModify, IfGoodTillCancelBidModified_WhenAloneOnBook_ThenPriceAndQuantityUpdated) {
    Orderbook orderbook{ std::chrono::hours{16} };
    constexpr OrderId bidId{0};
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, bidId, Side::Buy, Price{6.7}, Quantity{1}));

    const auto trades = orderbook.ModifyOrder(
        OrderModify{ bidId, Side::Buy, Price{8.0}, Quantity{3} });
    const auto orderInfo = orderbook.GetOrderInfos();

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderbook.Size(), std::size_t{1});
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{1});
    ASSERT_TRUE(orderInfo.asks_.empty());
    ASSERT_EQ(orderInfo.bids_.front().price_, Price{8.0});
    ASSERT_EQ(orderInfo.bids_.front().quantity_, Quantity{3});
}

TEST(OrderbookTestModify, IfGoodTillCancelBidModified_WhenAskAtSamePrice_ThenTradesGenerated) {
    Orderbook orderbook{ std::chrono::hours{16} };
    constexpr OrderId askId{0};
    constexpr OrderId bidId{1};
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, askId, Side::Sell, Price{6.7}, Quantity{2}));
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, bidId, Side::Buy, Price{4.2}, Quantity{2}));

    const auto trades = orderbook.ModifyOrder(
        OrderModify{ bidId, Side::Buy, Price{6.7}, Quantity{2} });
    const auto orderInfo = orderbook.GetOrderInfos();

    ASSERT_EQ(trades.size(), std::size_t{1});
    ASSERT_EQ(trades.front().GetBidTrade().orderId_, bidId);
    ASSERT_EQ(trades.front().GetAskTrade().orderId_, askId);
    ASSERT_EQ(trades.front().GetBidTrade().quantity_, Quantity{2});
    ASSERT_EQ(orderbook.Size(), std::size_t{0});
    ASSERT_TRUE(orderInfo.bids_.empty());
    ASSERT_TRUE(orderInfo.asks_.empty());
}

TEST(OrderbookTestModify, IfGoodTillCancelBidModified_WhenChangedToSell_ThenMovedToAskSide) {
    Orderbook orderbook{ std::chrono::hours{16} };
    constexpr OrderId orderId{0};
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, orderId, Side::Buy, Price{6.7}, Quantity{1}));

    const auto trades = orderbook.ModifyOrder(
        OrderModify{ orderId, Side::Sell, Price{7.0}, Quantity{2} });
    const auto orderInfo = orderbook.GetOrderInfos();

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderbook.Size(), std::size_t{1});
    ASSERT_TRUE(orderInfo.bids_.empty());
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{1});
    ASSERT_EQ(orderInfo.asks_.front().price_, Price{7.0});
    ASSERT_EQ(orderInfo.asks_.front().quantity_, Quantity{2});
}

TEST(OrderbookTestModify, IfGoodForDayBidModified_WhenAloneOnBook_ThenUpdatedOnBook) {
    Orderbook orderbook{ std::chrono::hours{16} };
    constexpr OrderId bidId{0};
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodForDay, bidId, Side::Buy, Price{6.7}, Quantity{1}));

    const auto trades = orderbook.ModifyOrder(
        OrderModify{ bidId, Side::Buy, Price{5.5}, Quantity{4} });
    const auto orderInfo = orderbook.GetOrderInfos();

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderbook.Size(), std::size_t{1});
    ASSERT_EQ(orderInfo.bids_.front().price_, Price{5.5});
    ASSERT_EQ(orderInfo.bids_.front().quantity_, Quantity{4});
}

TEST(OrderbookTestModify, IfGoodTillCancelBidModified_WhenPartiallyFilled_ThenReplacedWithNewQuantity) {
    Orderbook orderbook{ std::chrono::hours{16} };
    constexpr OrderId askId{0};
    constexpr OrderId bidId{1};
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, askId, Side::Sell, Price{6.7}, Quantity{2}));
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, bidId, Side::Buy, Price{6.7}, Quantity{5}));

    const auto trades = orderbook.ModifyOrder(
        OrderModify{ bidId, Side::Buy, Price{5.0}, Quantity{10} });
    const auto orderInfo = orderbook.GetOrderInfos();

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderbook.Size(), std::size_t{1});
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{1});
    ASSERT_TRUE(orderInfo.asks_.empty());
    ASSERT_EQ(orderInfo.bids_.front().price_, Price{5.0});
    ASSERT_EQ(orderInfo.bids_.front().quantity_, Quantity{10});
}

TEST(OrderbookTestModify, IfGoodTillCancelBidModified_WhenPriceChanges_ThenOldLevelRemoved) {
    Orderbook orderbook{ std::chrono::hours{16} };
    constexpr OrderId bidId{0};
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, bidId, Side::Buy, Price{6.7}, Quantity{1}));
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, OrderId{1}, Side::Buy, Price{4.2}, Quantity{1}));

    const auto trades = orderbook.ModifyOrder(
        OrderModify{ bidId, Side::Buy, Price{8.0}, Quantity{2} });
    const auto orderInfo = orderbook.GetOrderInfos();

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderbook.Size(), std::size_t{2});
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{2});
    ASSERT_EQ(orderInfo.bids_[0].price_, Price{8.0});
    ASSERT_EQ(orderInfo.bids_[0].quantity_, Quantity{2});
    ASSERT_EQ(orderInfo.bids_[1].price_, Price{4.2});
    ASSERT_EQ(orderInfo.bids_[1].quantity_, Quantity{1});
}

} // namespace
} // namespace OsborneX
