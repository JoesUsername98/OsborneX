#include "Orderbook/order_book.hpp"

#include <gtest/gtest.h>

namespace osbornex {
namespace {

TEST(OrderBookTest, EmptyByDefault) {
    OrderBook book;
    EXPECT_TRUE(book.empty());
}

} // namespace
} // namespace osbornex
