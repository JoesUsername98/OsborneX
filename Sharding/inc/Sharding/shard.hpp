#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <Orderbook/orderbook.hpp>

#include <Queue/ring_buffer.hpp>
#include <Messages/trade_execution.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Simulation {

class Shard
{
public:
    explicit Shard(
        std::size_t inbound_capacity = 4096,
        std::size_t outbound_capacity = 1024,
        std::size_t trade_outbound_capacity = 1024);

    Shard(const Shard&) = delete;
    Shard& operator=(const Shard&) = delete;

    void enqueue(OrderMessage message);
    void start();
    void stop();

    /// Drain remaining queued messages on the calling thread (for tests after stop).
    void drain();

    std::size_t book_size(SymbolId symbol) const;
    TopOfBookUpdate top_of_book(SymbolId symbol) const;
    bool has_book(SymbolId symbol) const;

    /// Outbound top-of-book stream. Consumers (e.g. PubSub::Subscriber) register
    /// their own cursor against this buffer -- Shard has no knowledge of who,
    /// if anyone, is reading it.
    Queue::RingBuffer<TopOfBookUpdate>& market_data_out() noexcept;

    /// Outbound trade-print stream, one entry per fill produced while matching.
    /// Same "Shard doesn't know who's reading" contract as market_data_out().
    Queue::RingBuffer<TradeExecution>& trade_out() noexcept;

private:
    void run();
    void process(const OrderMessage& message);
    TopOfBookUpdate make_top_of_book(SymbolId symbol, IngressSequence sequence) const;
    static bool top_of_book_equal(const TopOfBookUpdate& a, const TopOfBookUpdate& b);

    Queue::RingBuffer<OrderMessage> inbound_;
    Queue::RingBuffer<OrderMessage>::Consumer& inbound_cursor_;
    Queue::RingBuffer<TopOfBookUpdate> outbound_;
    Queue::RingBuffer<TradeExecution> trade_outbound_;

    // Guards books_/last_top_: process() (the worker thread) mutates both on every
    // message, while book_size()/has_book()/top_of_book() are called from whatever
    // thread owns the Shard (tests polling progress via wait_for) -- without this,
    // that's a concurrent unordered_map read/write, including a possible rehash
    // mid-read.
    mutable std::mutex books_mutex_;
    std::unordered_map<SymbolId, Orderbook> books_;
    std::unordered_map<SymbolId, TopOfBookUpdate> last_top_;
    std::atomic<bool> running_{ false };
    std::thread thread_;
};

} // namespace OsborneX::Simulation
