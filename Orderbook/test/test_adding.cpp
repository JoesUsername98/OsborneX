#include <gtest/gtest.h>
#include <chrono>

#include <Orderbook/orderbook.hpp>

namespace osbornex {
namespace {

TEST(OrderbookTestAdding, IfMarketBidAdded_WhenEmpty_ThenNoTradesNoOrdersStaysOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer myOrder = std::make_shared<Order>( OrderType::Market, OrderId{0uz}, Side::Buy, Constants::InvalidPrice, Quantity{1uz} );
    
    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 0uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 0uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 0uz });
    ASSERT_TRUE(trades.empty());
}

TEST(OrderbookTestAdding, IfGoodForDayBidAdded_WhenEmpty_ThenNoTradesOrdersStaysOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer myOrder = std::make_shared<Order>(OrderType::GoodForDay, OrderId{ 0uz }, Side::Buy, Price{ 6.7 }, Quantity{ 1uz });

    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 1uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 1uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 0uz });
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderInfo.bids_.front().price_, myOrder->GetPrice());
    ASSERT_EQ(orderInfo.bids_.front().quantity_, myOrder->GetRemainingQuantity());
}

TEST(OrderbookTestAdding, IfFillOrKillBidAdded_WhenEmpty_ThenNoTradesNoOrdersOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer myOrder = std::make_shared<Order>(OrderType::FillOrKill, OrderId{ 0uz }, Side::Buy, Price{ 6.7 }, Quantity{ 1uz });

    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 0uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 0uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 0uz });
    ASSERT_TRUE(trades.empty());
}

TEST(OrderbookTestAdding, IfGoodTillCancelBidAdded_WhenEmpty_ThenNoTradesOrdersStaysOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer myOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 0uz }, Side::Buy, Price{6.7}, Quantity{1uz});

    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 1uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 1uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 0uz });
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderInfo.bids_.front().price_, myOrder->GetPrice());
    ASSERT_EQ(orderInfo.bids_.front().quantity_, myOrder->GetRemainingQuantity());
}

TEST(OrderbookTestAdding, IfGoodTillCancelAskAdded_WhenEmpty_ThenNoTradesOrdersStaysOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer myOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 0uz }, Side::Sell, Price{ 6.7 }, Quantity{ 1uz });

    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 1uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 0uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 1uz });
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderInfo.asks_.front().price_, myOrder->GetPrice());
    ASSERT_EQ(orderInfo.asks_.front().quantity_, myOrder->GetRemainingQuantity());
}

TEST(OrderbookTestAdding, IfGoodTillCancelBidAdded_When1BidOnBookAtSameLevel_ThenNoTradesOrdersStaysOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer existingOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 0uz }, Side::Buy, Price{ 6.7 }, Quantity{ 1uz });
    myOrderbook.AddOrder(existingOrder);
    OrderPointer myOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 1uz }, Side::Buy, Price{ 6.7 }, Quantity{ 1uz });

    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 2uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 1uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 0uz });
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderInfo.bids_.front().price_, myOrder->GetPrice());
    ASSERT_EQ(orderInfo.bids_.front().quantity_, myOrder->GetRemainingQuantity() + existingOrder->GetRemainingQuantity());
}

TEST(OrderbookTestAdding, IfGoodTillCancelBidAdded_When1BidOnBookAtWorseLevel_ThenNoTradesOrdersStaysOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer existingOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 0uz }, Side::Buy, Price{ 4.2 }, Quantity{ 1uz });
    myOrderbook.AddOrder(existingOrder);
    OrderPointer myOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 1uz }, Side::Buy, Price{ 6.7 }, Quantity{ 1uz });

    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 2uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 2uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 0uz });
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderInfo.bids_[0].price_, myOrder->GetPrice());
    ASSERT_EQ(orderInfo.bids_[0].quantity_, myOrder->GetRemainingQuantity());
    ASSERT_EQ(orderInfo.bids_[1].price_, existingOrder->GetPrice());
    ASSERT_EQ(orderInfo.bids_[1].quantity_, existingOrder->GetRemainingQuantity());
}

TEST(OrderbookTestAdding, IfGoodTillCancelBidAdded_When1AskOnBookAtWorseLevel_ThenNoTradesOrdersStaysOnBook) {
    // Arrange
    Orderbook myOrderbook{ std::chrono::hours{16} };
    OrderPointer existingOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 0uz }, Side::Sell, Price{ 6.7 }, Quantity{ 1uz });
    myOrderbook.AddOrder(existingOrder);
    OrderPointer myOrder = std::make_shared<Order>(OrderType::GoodTillCancel, OrderId{ 1uz }, Side::Buy, Price{ 4.2 }, Quantity{ 1uz });

    // Act 
    const auto trades = myOrderbook.AddOrder(myOrder);
    const auto orderBookSize = myOrderbook.Size();
    const auto orderInfo = myOrderbook.GetOrderInfos();

    // Assert 
    ASSERT_EQ(orderBookSize, std::size_t{ 2uz });
    ASSERT_EQ(orderInfo.bids_.size(), std::size_t{ 1uz });
    ASSERT_EQ(orderInfo.asks_.size(), std::size_t{ 1uz });
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(orderInfo.bids_.front().price_, myOrder->GetPrice());
    ASSERT_EQ(orderInfo.bids_.front().quantity_, myOrder->GetRemainingQuantity());
    ASSERT_EQ(orderInfo.asks_.front().price_, existingOrder->GetPrice());
    ASSERT_EQ(orderInfo.asks_.front().quantity_, existingOrder->GetRemainingQuantity());
}

} // namespace
} // namespace osbornex
