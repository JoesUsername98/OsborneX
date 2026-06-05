#pragma once
#include <expected>
#include <string>
#include <format>
#include <vector>
#include <memory>
#include <list>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <numeric>

#include "domain_types.hpp"
#include "trade.hpp"
#include "order_type.hpp"

namespace osbornex {

enum class Side
{
    Buy,
    Sell
};

struct LevelInfo 
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

struct OrderbookLevelInfos 
{
    LevelInfos bids_;
    LevelInfos asks_;
};

class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
    : orderType_{ orderType }
    , orderId_{ orderId }
    , side_ { side }
    , price_ { price }
    , initialQuantity_ { quantity }
    , remainingQuantity_ { quantity }
    { }

    OrderType GetOrderType() const { return orderType_; }
    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_;  }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const{ return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return initialQuantity_ - remainingQuantity_; }
    bool IsFilled() const { return initialQuantity_ == remainingQuantity_;  }

    /*[[nodiscard]] std::expected<void, std::string>*/
    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
            return; /*std::unexpected(
                std::format("OrderID [{}] cannot be filled as the fill quantity [{}] is greater than the remaining quantity [{}]",
                GetOrderId(), quantity, GetRemainingQuantity()) 
            );*/

        remainingQuantity_ -= quantity;
    }


private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity )
        : orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , quantity_ { quantity }
    { }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType orderType)
    {
        return std::make_shared<Order>(orderType, orderId_, side_, price_, quantity_);
    }

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

class Orderbook
{
public:
    Trades AddOrder(OrderPointer order)
    {
        if (orders_.contains(order->GetOrderId()))
            return {};

        if (order->GetOrderType() == OrderType::FillAndKill &&
            !CanMatch(order->GetSide(), order->GetPrice()))
            return {};

        OrderPointers::iterator iterator;

        switch (order->GetSide())
        {
        case Side::Buy:
        {
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
            break;
        }
        case Side::Sell:
        {
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
            break;
        }
        default:
            std::unreachable();
        }

        orders_.emplace(
            order->GetOrderId(),
            OrderEntry
            {
                .order_ = order,
                .location_ = iterator
            }
        );

        return MatchOrders();
    }

    // Make std::expected<orderId,std::string>
    void CancelOrder(OrderId orderId)
    {
        if (!orders_.contains(orderId))
            return; // std::unexpected(std::format("Cannot cancel order [{}] that is not in orderbook", orderId));

        const auto& [order, iterator] = orders_.at(orderId);
        orders_.erase(orderId);

        switch (order->GetSide())
        {
        case Side::Buy:
        {
            auto price = order->GetPrice();
            auto& bidsAtLevel = bids_.at(price);
            bidsAtLevel.erase(iterator);
            if (bidsAtLevel.empty())
                bids_.erase(price);
            break;
        }
        case Side::Sell:
        {
            auto price = order->GetPrice();
            auto& asksAtPrice = asks_.at(price);
            asksAtPrice.erase(iterator);
            if (asksAtPrice.empty())
                asks_.erase(price);
            break;
        }
        default:
            std::unreachable();
        }
    }

    Trades MatchOrder(OrderModify order)
    {
        if (!orders_.contains(order.GetOrderId()))
            return {};

        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return orders_.size(); }
    
    OrderbookLevelInfos GetOrderInfos()
    {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
            {
                return LevelInfo{
                    .price_ = price,
                    .quantity_ = std::accumulate(orders.begin(), orders.end(), Quantity{0},
                        [](Quantity runningSum, const OrderPointer& order)
                        { return runningSum + order->GetRemainingQuantity(); }
                    )
                };
            };

        for (const auto [price, bidsAtPrice] : bids_)
            bidInfos.push_back(CreateLevelInfos(price, bidsAtPrice));

        for (const auto [price, asksAtPrice] : asks_)
            askInfos.push_back(CreateLevelInfos(price, asksAtPrice));

        return OrderbookLevelInfos{
            .bids_ = bidInfos,
            .asks_ = askInfos
        };
    }

private:
    struct OrderEntry
    {
        OrderPointer order_{};
        OrderPointers::iterator location_;
    };

    using BidLevels = std::map<Price, OrderPointers, std::greater<Price>>;
    using AskLevels = std::map<Price, OrderPointers, std::less<Price>>;
    using Orders = std::unordered_map<OrderId, OrderEntry>;

    BidLevels bids_;
    AskLevels asks_;
    Orders orders_;

    bool CanMatch(Side side, Price price) const
    {
        switch (side)
        {
        case Side::Buy:
        {
            if (asks_.empty())
                return false;

            const auto& [bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        }
        case Side::Sell:
        {
            if (bids_.empty())
                return false;

            const auto& [bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
        default:
            std::unreachable();
        }
    }

    Trades MatchOrders()
    {
        Trades  trades;
        trades.reserve(orders_.size());

        while (true)
        {
            if (bids_.empty() || asks_.empty())
                break;

            auto& [bidPrice, bidsAtPrice] = *bids_.begin();
            auto& [askPrice, asksAtPrice] = *asks_.begin();

            if (bidPrice < askPrice)
                break;

            while (bidsAtPrice.size() && asksAtPrice.size())
            {
                auto& bid = bidsAtPrice.front();
                auto& ask = asksAtPrice.front();

                Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
                bid->Fill(quantity);
                ask->Fill(quantity);

                if (bid->IsFilled())
                {
                    bidsAtPrice.pop_front();
                    orders_.erase(bid->GetOrderId());
                    if (bidsAtPrice.empty())
                        bids_.erase(bidPrice);
                }

                if (ask->IsFilled())
                {
                    asksAtPrice.pop_front();
                    orders_.erase(ask->GetOrderId());
                    if (asksAtPrice.empty())
                        asks_.erase(askPrice);
                }

                trades.push_back(Trade{
                        TradeInfo
                        {
                            .orderId_ = bid->GetOrderId(),
                            .price_ = bid->GetPrice(),
                            .quantity_ = quantity
                        },
                        TradeInfo
                        {
                            .orderId_ = ask->GetOrderId(),
                            .price_ = ask->GetPrice(),
                            .quantity_ = quantity
                        }
                    }
                    );
            }

            if (!bids_.empty())
            {
                auto& [_, bids] = *bids_.begin();
                auto& order = bids.front();
                if (order->GetOrderType() == OrderType::FillAndKill)
                    CancelOrder(order->GetOrderId());
            }

            if (!asks_.empty())
            {
                auto& [_, asks] = *asks_.begin();
                auto& order = asks.front();
                if (order->GetOrderType() == OrderType::FillAndKill)
                    CancelOrder(order->GetOrderId());
            }

        }

        return trades;
    }


};


/// <summary>
/// Get rid of me, I am a redundant stub for benchmarking and testing
/// </summary>
class OrderBook {
public:
    [[nodiscard]] bool empty() const noexcept;
};

} // namespace osbornex
