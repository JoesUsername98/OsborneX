#include "Orderbook/orderbook.hpp"

#include <algorithm>
#include <numeric>
#include <optional>

namespace OsborneX {

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
    }

    return MatchOrders();
}

std::chrono::system_clock::time_point Orderbook::GetNextMarketClose(std::chrono::system_clock::time_point asof) const
{
    using namespace std::chrono;
    const zoned_time zoned{ current_zone(), asof};
    const auto localNow = zoned.get_local_time();
    const local_days day{ floor<days>(localNow)};
    auto close = local_seconds{day} + marketCloseHour_;

    if (localNow >= close)
        close += days{1};

    return zoned_time{ current_zone(), close}.get_sys_time();
}

void Orderbook::PruneGoodForDayOrders()
{
    while (true)
    {
        {
            std::unique_lock ordersLock{ ordersMutex_ };

            const bool shuttingDown = shutdownConditionVariable_.wait_until(
                ordersLock,
                GetNextMarketClose(),
                [this] { return shutdown_.load(std::memory_order_acquire); });

            if (shuttingDown)
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

    OnOrderCancelled(order);
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
    std::scoped_lock ordersLock{ ordersMutex_ };
    Trades trades;
    trades.reserve(orders_.size());

    while (true) 
    {
        if (bids_.empty() || asks_.empty())
            break;

        const Price bestBidPrice = bids_.begin()->first;
        const Price bestAskPrice = asks_.begin()->first;

        if (bestBidPrice < bestAskPrice)
            break;

        auto& bidsAtBestPrice = bids_.at(bestBidPrice);
        auto& asksAtBestPrice = asks_.at(bestAskPrice);

        while (!bidsAtBestPrice.empty() && !asksAtBestPrice.empty()) 
        {
            const OrderPointer bid = bidsAtBestPrice.front();
            const OrderPointer ask = asksAtBestPrice.front();

            const Quantity quantity =
                std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
            bid->Fill(quantity);
            ask->Fill(quantity);

            const OrderId bidId = bid->GetOrderId();
            const OrderId askId = ask->GetOrderId();
            const Price bidPrice = bid->GetPrice();
            const Price askPrice = ask->GetPrice();
            const bool bidFilled = bid->IsFilled();
            const bool askFilled = ask->IsFilled();

            if (bidFilled) 
            {
                bidsAtBestPrice.pop_front();
                orders_.erase(bidId);
            }

            if (askFilled) 
            {
                asksAtBestPrice.pop_front();
                orders_.erase(askId);
            }

            trades.push_back(Trade{
                TradeInfo{
                    .orderId_ = bidId,
                    .price_ = bidPrice,
                    .quantity_ = quantity,
                },
                TradeInfo{
                    .orderId_ = askId,
                    .price_ = askPrice,
                    .quantity_ = quantity,
                },
            });

            OnOrderMatched(bidPrice, quantity, bidFilled);
            OnOrderMatched(askPrice, quantity, askFilled);
        }

        if (bidsAtBestPrice.empty())
        {
            bids_.erase(bestBidPrice);
            levelData_.erase(bestBidPrice);
        }
    
        if (asksAtBestPrice.empty())
        {
            asks_.erase(bestAskPrice);
            levelData_.erase(bestAskPrice);
        }

        if (!bids_.empty()) 
        {
            auto& [_, dirtyBids] = *bids_.begin();
            auto& bidOrder = dirtyBids.front();
            if (bidOrder->GetOrderType() == OrderType::FillAndKill)
                CancelOrderInternal(bidOrder->GetOrderId());
        }

        if (!asks_.empty()) 
        {
            auto& [_, dirtAsks] = *asks_.begin();
            auto& askOrder = dirtAsks.front();
            if (askOrder->GetOrderType() == OrderType::FillAndKill)
                CancelOrderInternal(askOrder->GetOrderId());
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


} // namespace OsborneX
