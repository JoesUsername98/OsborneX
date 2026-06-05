#include "Orderbook/orderbook.hpp"

#include <algorithm>
#include <numeric>

namespace osbornex {

Trades Orderbook::AddOrder(OrderPointer order) 
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
        auto& bidsAtPrice = bids_[order->GetPrice()];
        bidsAtPrice.push_back(order);
        iterator = std::next(bidsAtPrice.begin(), bidsAtPrice.size() - 1);
        break;
    }
    case Side::Sell: 
    {
        auto& asksAtPrice = asks_[order->GetPrice()];
        asksAtPrice.push_back(order);
        iterator = std::next(asksAtPrice.begin(), asksAtPrice.size() - 1);
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

    return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId) 
{
    if (!orders_.contains(orderId))
        return;

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

    OnOrderCancelled(orders_.at(orderId).order_);
}

Trades Orderbook::ModifyOrder(OrderModify order) 
{
    if (!orders_.contains(order.GetOrderId()))
        return {};

    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}

std::size_t Orderbook::Size() const 
{
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
