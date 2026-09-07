#include <Simulation/shard.hpp>
#include <Simulation/publisher.hpp>

#include <cmath>
#include <memory>

#include <Orderbook/order.hpp>
#include <Orderbook/order_modify.hpp>

namespace OsborneX::Simulation {
namespace {

bool SamePrice(Price a, Price b)
{
    if (std::isnan(a) && std::isnan(b))
        return true;
    return a == b;
}

} // namespace

Shard::Shard(MarketDataPublisher& publisher, std::size_t queue_capacity)
    : queue_(queue_capacity)
    , publisher_(publisher)
{
}

void Shard::enqueue(OrderMessage message)
{
    while (!queue_.try_push(message))
        std::this_thread::yield();
}

void Shard::start()
{
    if (running_.exchange(true))
        return;

    thread_ = std::thread(&Shard::run, this);
}

void Shard::stop()
{
    if (!running_.exchange(false))
        return;

    if (thread_.joinable())
        thread_.join();

    drain();
}

void Shard::drain()
{
    OrderMessage message;
    while (queue_.try_pop(message))
        process(message);
}

std::size_t Shard::book_size(SymbolId symbol) const
{
    const auto it = books_.find(symbol);
    if (it == books_.end())
        return 0;
    return it->second.Size();
}

bool Shard::has_book(SymbolId symbol) const
{
    return books_.contains(symbol);
}

TopOfBookUpdate Shard::top_of_book(SymbolId symbol) const
{
    return make_top_of_book(symbol, 0);
}

void Shard::run()
{
    while (running_.load(std::memory_order_acquire))
    {
        OrderMessage message;
        if (!queue_.try_pop(message))
        {
            std::this_thread::yield();
            continue;
        }

        process(message);
    }
}

void Shard::process(const OrderMessage& message)
{
    auto& book = books_[message.symbol];

    switch (message.action)
    {
    case OrderAction::Add:
        book.AddOrder(std::make_shared<Order>(
            message.type,
            message.order_id,
            message.side,
            message.price,
            message.quantity));
        break;
    case OrderAction::Cancel:
        book.CancelOrder(message.order_id);
        break;
    case OrderAction::Modify:
        book.ModifyOrder(OrderModify{
            message.order_id,
            message.side,
            message.price,
            message.quantity });
        break;
    }

    const auto update = make_top_of_book(message.symbol, message.ingress_sequence);
    auto& last = last_top_[message.symbol];
    if (!top_of_book_equal(last, update))
    {
        last = update;
        publisher_.publish(update);
    }
}

TopOfBookUpdate Shard::make_top_of_book(SymbolId symbol, IngressSequence sequence) const
{
    TopOfBookUpdate update{
        .symbol = symbol,
        .sequence = sequence,
    };

    const auto it = books_.find(symbol);
    if (it == books_.end())
        return update;

    const auto infos = it->second.GetOrderInfos();
    if (!infos.bids_.empty())
    {
        update.bid_price = infos.bids_.front().price_;
        update.bid_quantity = infos.bids_.front().quantity_;
    }
    if (!infos.asks_.empty())
    {
        update.ask_price = infos.asks_.front().price_;
        update.ask_quantity = infos.asks_.front().quantity_;
    }

    return update;
}

bool Shard::top_of_book_equal(const TopOfBookUpdate& a, const TopOfBookUpdate& b)
{
    return a.symbol == b.symbol
        && SamePrice(a.bid_price, b.bid_price)
        && a.bid_quantity == b.bid_quantity
        && SamePrice(a.ask_price, b.ask_price)
        && a.ask_quantity == b.ask_quantity;
}

} // namespace OsborneX::Simulation
