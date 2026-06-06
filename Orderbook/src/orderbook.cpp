#include "Orderbook/orderbook.hpp"

#include <algorithm>
#include <numeric>
#include <chrono>
#include <optional>

namespace osbornex {

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (!CanMatch(side, price))
        return false;

    std::optional<Price> threshold;

    switch (side)
    {
    case Side::Buy:
    {
        const auto [askPrice, _] = *asks_.begin();
        threshold = askPrice;
        break;
    }
    case Side::Sell:
    {
        const auto [bidPrice, _] = *bids_.begin();
        threshold = bidPrice;
        break;
    }
    default:
        std::unreachable();
    }

    for (const auto& [levelPrice, levelData] : levelData_)
    {
        if (threshold.has_value() &&
            (side == Side::Buy && threshold.value() > levelPrice) ||
            (side == Side::Sell && threshold.value() < levelPrice))
            continue;

        if ((side == Side::Buy && levelPrice > price) ||
            (side == Side::Sell && levelPrice < price))
            continue;

        if (quantity <= levelData.quantity_)
            return true;

        quantity -= levelData.quantity_;
    }

    return false;
}

Trades Orderbook::AddOrder(OrderPointer order) 
{
    std::scoped_lock ordersLock{ ordersMutex_ };

    if (orders_.contains(order->GetOrderId()))
        return {};

    if (order->GetOrderType() == OrderType::FillAndKill &&
        !CanMatch(order->GetSide(), order->GetPrice()))
        return {};

    if (order->GetOrderType() == OrderType::Market)
    {
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto& [worstAsk, _] = *asks_.rbegin();
            order->ToGoodTillCancel(worstAsk);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto& [worstBid, _] = *bids_.rbegin();
            order->ToGoodTillCancel(worstBid);
        }
        else
            return { };
    }

    if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
        return { };

    OrderPointers::iterator iterator;

    switch (order->GetSide()) 
    {
    case Side::Buy: 
    {
        auto& bidsAtPrice = bids_[order->GetPrice()];
        bidsAtPrice.push_back(order);
        iterator = std::prev(bidsAtPrice.end());
        break;
    }
    case Side::Sell: 
    {
        auto& asksAtPrice = asks_[order->GetPrice()];
        asksAtPrice.push_back(order);
        iterator = std::prev(asksAtPrice.end());
        break;
    }
    default:
        std::unreachable();
    }

    orders_.emplace(
        order->GetOrderId(),
        OrderEntry{
            .order_ = order,
            .location_ = iterator,
        });
    
    OnOrderAdded(order);
    
    return MatchOrders();
}

void Orderbook::PruneGoodForDayOrders()
{
    using namespace std::chrono;
    const auto end = hours(16);

    while (true)
    {
        const auto now = system_clock::now();
        const auto now_c = system_clock::to_time_t(now);
        std::tm now_parts;
        localtime_s(&now_parts, &now_c);

        if (now_parts.tm_hour >= end.count())
            now_parts.tm_mday += 1;

        now_parts.tm_hour = end.count();
        now_parts.tm_min = 0;
        now_parts.tm_sec = 0;

        auto next = system_clock::from_time_t(mktime(&now_parts));
        auto till = next - now + milliseconds(100);

        {
            std::unique_lock ordersLock{ ordersMutex_ };

            if (shutdown_.load(std::memory_order_acquire) ||
                shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
                return;
        }

        OrderIds orderIds;
        orderIds.reserve(orders_.size());

        {
            std::scoped_lock ordersLock{ ordersMutex_ };

            for (const auto& [_, entry] : orders_)
            {
                const auto& order = entry.order_;

                if (order->GetOrderType() != OrderType::GoodForDay)
                    continue;

                orderIds.push_back(order->GetOrderId());
            }
        }

        CancelOrders(orderIds);
    }
}

void Orderbook::CancelOrders(OrderIds orderIds)
{
    std::scoped_lock ordersLock{ ordersMutex_ };

    for (const auto& orderId : orderIds)
        CancelOrderInternal(orderId);
}

void Orderbook::CancelOrder(OrderId orderId) 
{
    std::scoped_lock ordersLock{ ordersMutex_ };

    CancelOrderInternal(orderId);
}

void Orderbook::CancelOrderInternal(OrderId orderId) 
{
    if (!orders_.contains(orderId))
        return;

    const auto [order, iterator] = orders_.at(orderId);
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

    OnOrderCancelled(orders_.at(orderId).order_);
}

Trades Orderbook::ModifyOrder(OrderModify order) 
{
    OrderType orderType;

    {
        std::scoped_lock ordersLock{ ordersMutex_ };

        if (!orders_.contains(order.GetOrderId()))
            return { };

        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        orderType = existingOrder->GetOrderType();
    }

    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(orderType));
}

std::size_t Orderbook::Size() const 
{
    std::scoped_lock ordersLock{ ordersMutex_ };
    return orders_.size();
}

OrderbookLevelInfos Orderbook::GetOrderInfos() 
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders) 
    {
        return LevelInfo{
            .price_ = price,
            .quantity_ = std::accumulate(
                orders.begin(),
                orders.end(),
                Quantity{0},
                [](Quantity runningSum, const OrderPointer& order) {
                    return runningSum + order->GetRemainingQuantity();
                }),
        };
    };

    for (const auto [price, bidsAtPrice] : bids_)
        bidInfos.push_back(CreateLevelInfos(price, bidsAtPrice));

    for (const auto [price, asksAtPrice] : asks_)
        askInfos.push_back(CreateLevelInfos(price, asksAtPrice));

    return OrderbookLevelInfos{
        .bids_ = bidInfos,
        .asks_ = askInfos,
    };
}

bool Orderbook::CanMatch(Side side, Price price) const 
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

Trades Orderbook::MatchOrders() 
{
    Trades trades;
    trades.reserve(orders_.size());

    while (true) 
    {
        if (bids_.empty() || asks_.empty())
            break;

        auto& [bestBidPrice, bidsAtBestPrice] = *bids_.begin();
        auto& [bestAskPrice, asksAtBestPrice] = *asks_.begin();

        if (bestBidPrice < bestAskPrice)
            break;

        while (bidsAtBestPrice.size() && asksAtBestPrice.size()) 
        {
            auto& bid = bidsAtBestPrice.front();
            auto& ask = asksAtBestPrice.front();

            Quantity quantity =
                std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
            bid->Fill(quantity);
            ask->Fill(quantity);

            if (bid->IsFilled()) 
            {
                bidsAtBestPrice.pop_front();
                orders_.erase(bid->GetOrderId());
                if (bidsAtBestPrice.empty())
                    bids_.erase(bestBidPrice);
            }

            if (ask->IsFilled()) {
                asksAtBestPrice.pop_front();
                orders_.erase(ask->GetOrderId());
                if (asksAtBestPrice.empty())
                    asks_.erase(bestAskPrice);
            }

            trades.push_back(Trade{
                TradeInfo{
                    .orderId_ = bid->GetOrderId(),
                    .price_ = bid->GetPrice(),
                    .quantity_ = quantity,
                },
                TradeInfo{
                    .orderId_ = ask->GetOrderId(),
                    .price_ = ask->GetPrice(),
                    .quantity_ = quantity,
                },
            });

            OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
            OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());
        }

        if (!bids_.empty()) 
        {
            auto& [_, dirtyBids] = *bids_.begin();
            auto& bidOrder = dirtyBids.front();
            if (bidOrder->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(bidOrder->GetOrderId());
        }

        if (!asks_.empty()) 
        {
            auto& [_, dirtAsks] = *asks_.begin();
            auto& askOrder = dirtAsks.front();
            if (askOrder->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(askOrder->GetOrderId());
        }
    }

    return trades;
}

void Orderbook::OnOrderCancelled(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void Orderbook::OnOrderAdded(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled)
{
    UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
    auto& data = levelData_[price];

    data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1 : 0;
    if (action == LevelData::Action::Remove || action == LevelData::Action::Match)
    {
        data.quantity_ -= quantity;
    }
    else
    {
        data.quantity_ += quantity;
    }

    if (data.count_ == 0)
        levelData_.erase(price);
}


} // namespace osbornex
