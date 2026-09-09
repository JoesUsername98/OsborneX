#include <gtest/gtest.h>

#include <Orderbook/orderbook.hpp>

namespace OsborneX {
namespace {

// Orderbook is intentionally single-threaded (see CLAUDE.md) -- concurrency
// is handled entirely by the Sharding library, which gives each Orderbook
// instance its own dedicated worker thread. Genuine "one Orderbook, one
// thread" coverage lives in Sharding/test/test_shard.cpp; these are plain
// single-threaded construct/destroy checks.

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
