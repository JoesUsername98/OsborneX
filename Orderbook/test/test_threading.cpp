#include <gtest/gtest.h>
#include <chrono>

#include <Orderbook/orderbook.hpp>


namespace OsborneX {
namespace {

TEST(OrderbookTestThreading, IfShutdown_WhenPruneThreadNotWaiting_ThenIsGraceful) {
    Orderbook myOrderbook{ std::chrono::hours{16} };
}

TEST(OrderbookTestThreading, IfShutdown_WhenPruneThreadWaiting_ThenIsGraceful) {
    Orderbook myOrderbook{std::chrono::hours{16}};
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

} // namespace
} // namespace OsborneX
