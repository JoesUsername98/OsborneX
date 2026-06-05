#pragma once

#include <map>
#include <unordered_map>

#include "order.hpp"
#include "order_modify.hpp"
#include "orderbook_level_info.hpp"
#include "trade.hpp"
#include <mutex>

namespace osbornex {

class Orderbook 
{
public:
    Orderbook() : ordersPruneThread_{ [this] { PruneGoodForDayOrders(); } } 
    {}
    ~Orderbook()
    {
        shutdownConditionVariable_.notify_one();
        ordersPruneThread_.join();
    }
    Orderbook(const Orderbook&) = delete;
    void operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    void operator=(Orderbook&&) = delete;

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);
    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos();

private:
    struct OrderEntry 
    {
        OrderPointer order_{};
        OrderPointers::iterator location_;
    };

    struct LevelData
    {
        Quantity quantity_{ };
        Quantity count_{ };

        enum class Action
        {
            Add,
            Remove,
            Match
        };
    };

    using BidLevels = std::map<Price, OrderPointers, std::greater<Price>>;
    using AskLevels = std::map<Price, OrderPointers, std::less<Price>>;
    using Orders = std::unordered_map<OrderId, OrderEntry>;
    using LevelDataMap = std::unordered_map<Price, LevelData>;

    LevelDataMap levelData_;
    BidLevels bids_;
    AskLevels asks_;
    Orders orders_;

    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;

    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    void PruneGoodForDayOrders();

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);
};

} // namespace osbornex
