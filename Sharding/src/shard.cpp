#include <Sharding/shard.hpp>

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

Shard::Shard(std::size_t inbound_capacity, std::size_t outbound_capacity, std::size_t trade_outbound_capacity)
    : inbound_(inbound_capacity)
    , inbound_cursor_(inbound_.add_consumer(Queue::ConsumerPolicy::Lossless))
    , outbound_(outbound_capacity)
    , trade_outbound_(trade_outbound_capacity)
{
}

void Shard::enqueue(OrderMessage message)
{
    inbound_.push_blocking(std::move(message));
}

void Shard::start()
{
    if (running_.exchange(true))
        return;

    // A prior stop() may have left the calling thread as outbound_'s/
    // trade_outbound_'s recorded producer (see stop()'s comment) -- release
    // that so the new worker thread can freely become the producer for this run.
    outbound_.reset_producer_thread();
    trade_outbound_.reset_producer_thread();
    thread_ = std::thread(&Shard::run, this);
}

void Shard::stop()
{
    if (!running_.exchange(false))
        return;

    if (thread_.joinable())
        thread_.join();

    // The worker thread has fully stopped -- join() establishes a
    // happens-before edge -- so it's safe, and expected, for this (likely
    // different) calling thread to take over as outbound_'s/trade_outbound_'s
    // producer while drain() publishes any top-of-book changes and trade
    // prints left over from processing whatever remained in the inbound queue.
    outbound_.reset_producer_thread();
    trade_outbound_.reset_producer_thread();
    drain();
}

void Shard::drain()
{
    OrderMessage message;
    while (inbound_cursor_.try_read(message) == Queue::ReadResult::Ok)
        process(message);
}

std::size_t Shard::book_size(SymbolId symbol) const
{
    std::lock_guard lock{ books_mutex_ };
    const auto it = books_.find(symbol);
    if (it == books_.end())
        return 0;
    return it->second.Size();
}

bool Shard::has_book(SymbolId symbol) const
{
    std::lock_guard lock{ books_mutex_ };
    return books_.contains(symbol);
}

TopOfBookUpdate Shard::top_of_book(SymbolId symbol) const
{
    std::lock_guard lock{ books_mutex_ };
    return make_top_of_book(symbol, 0);
}

void Shard::run()
{
    while (running_.load(std::memory_order_acquire))
    {
        OrderMessage message;
        if (inbound_cursor_.try_read(message) != Queue::ReadResult::Ok)
        {
            std::this_thread::yield();
            continue;
        }

        process(message);
    }
}

void Shard::process(const OrderMessage& message)
{
    Trades trades;
    TopOfBookUpdate update{};
    bool top_changed = false;

    {
        // books_/last_top_ are also read by book_size()/has_book()/top_of_book()
        // from whichever thread owns the Shard, so every touch of either needs
        // this lock held -- process() runs on the worker thread only, but those
        // accessors don't.
        std::lock_guard lock{ books_mutex_ };
        auto& book = books_[message.symbol];

        switch (message.action)
        {
        case OrderAction::Add:
            trades = book.AddOrder(std::make_shared<Order>(
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
            trades = book.ModifyOrder(OrderModify{
                message.order_id,
                message.side,
                message.price,
                message.quantity });
            break;
        }

        update = make_top_of_book(message.symbol, message.ingress_sequence);
        auto& last = last_top_[message.symbol];
        if (!top_of_book_equal(last, update))
        {
            last = update;
            top_changed = true;
        }
    }

    for (const auto& trade : trades)
    {
        const auto& bid = trade.GetBidTrade();
        const auto& ask = trade.GetAskTrade();
        trade_outbound_.push_overwrite(TradeExecution{
            .symbol = message.symbol,
            .sequence = message.ingress_sequence,
            .timestamp = message.ingress_timestamp,
            .bid_order_id = bid.orderId_,
            .bid_price = bid.price_,
            .ask_order_id = ask.orderId_,
            .ask_price = ask.price_,
            .quantity = bid.quantity_,
        });
    }

    if (top_changed)
        outbound_.push_overwrite(update);
}

// Caller must hold books_mutex_ -- this only reads books_, never locks itself, so
// both process() (already inside the lock) and top_of_book() (takes it first) can
// call it without a recursive-lock deadlock.
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

Queue::RingBuffer<TopOfBookUpdate>& Shard::market_data_out() noexcept
{
    return outbound_;
}

Queue::RingBuffer<TradeExecution>& Shard::trade_out() noexcept
{
    return trade_outbound_;
}

} // namespace OsborneX::Simulation
