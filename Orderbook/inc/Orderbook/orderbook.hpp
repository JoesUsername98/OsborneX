#pragma once

#include <map>
#include <unordered_map>

#include "order.hpp"
#include "order_modify.hpp"
#include "orderbook_level_info.hpp"
#include "trade.hpp"

namespace osbornex {

class Orderbook {
public:
    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);
    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos();

private:
    struct OrderEntry {
        OrderPointer order_{};
        OrderPointers::iterator location_;
    };

    using BidLevels = std::map<Price, OrderPointers, std::greater<Price>>;
    using AskLevels = std::map<Price, OrderPointers, std::less<Price>>;
    using Orders = std::unordered_map<OrderId, OrderEntry>;

    BidLevels bids_;
    AskLevels asks_;
    Orders orders_;

    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
};

} // namespace osbornex
