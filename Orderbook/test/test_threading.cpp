#include <gtest/gtest.h>

#include <Orderbook/orderbook.hpp>

namespace OsborneX {
namespace {

TEST(OrderbookTestLifecycle, IfConstructed_WhenDestroyed_ThenNoHang) {
    Orderbook myOrderbook{};
}

TEST(OrderbookTestLifecycle, IfOrdersAdded_WhenDestroyed_ThenNoHang) {
    Orderbook myOrderbook{};
    myOrderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, 1, Side::Buy, 100.0, 10));
}

} // namespace
} // namespace OsborneX
