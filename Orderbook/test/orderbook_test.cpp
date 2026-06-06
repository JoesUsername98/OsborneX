#include <gtest/gtest.h>
#include <chrono>

#include <Orderbook/orderbook.hpp>


namespace osbornex {
namespace {

TEST(OrderBookTest, EmptyByDefault) {
    Orderbook myOrderbook;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ( myOrderbook.Size(),  Quantity{ 0 } );
}

} // namespace
} // namespace osbornex
