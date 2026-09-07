#pragma once

#include <map>
#include <unordered_map>
#include <chrono>

#include "order.hpp"
#include "order_modify.hpp"
#include "orderbook_level_info.hpp"
#include "trade.hpp"

namespace OsborneX {

class Orderbook
{
public:
    explicit Orderbook(std::chrono::hours closeHour = std::chrono::hours{16})
        : marketCloseHour_{ closeHour }
    {
    }

    Orderbook(const Orderbook&) = delete;
    Orderbook& operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = default;
    Orderbook& operator=(Orderbook&&) = default;

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);
    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;

    /// @brief Returns the next market close after @p asof.
    /// @details Computes the next local-time occurrence of @c marketCloseHour_.
    ///          Weekends and holidays are not excluded; every calendar day is treated as a business day.
    /// @param asof The reference time. Defaults to the current time.
    /// @return The next market close as a @c std::chrono::system_clock time point.
    /// @note Close time is interpreted in the system's local timezone.
    std::chrono::system_clock::time_point GetNextMarketClose(
        std::chrono::system_clock::time_point asof = std::chrono::system_clock::now()) const;

private:
    struct OrderEntry
    {
        OrderPointer order_{};
        OrderPointers::iterator location_;
    };

    struct LevelData
    {
        Quantity quantity_{};
        Quantity count_{};

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

    std::chrono::hours marketCloseHour_;

    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);
};

} // namespace OsborneX
