#pragma once

#include <atomic>
#include <thread>
#include <unordered_map>

#include <Orderbook/orderbook.hpp>

#include <Queue/ring_buffer.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Simulation {

class Shard
{
public:
    explicit Shard(std::size_t inbound_capacity = 4096, std::size_t outbound_capacity = 1024);

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

private:
    void run();
    void process(const OrderMessage& message);
    TopOfBookUpdate make_top_of_book(SymbolId symbol, IngressSequence sequence) const;
    static bool top_of_book_equal(const TopOfBookUpdate& a, const TopOfBookUpdate& b);

    Queue::RingBuffer<OrderMessage> inbound_;
    Queue::RingBuffer<OrderMessage>::Consumer& inbound_cursor_;
    Queue::RingBuffer<TopOfBookUpdate> outbound_;
    std::unordered_map<SymbolId, Orderbook> books_;
    std::unordered_map<SymbolId, TopOfBookUpdate> last_top_;
    std::atomic<bool> running_{ false };
    std::thread thread_;
};

} // namespace OsborneX::Simulation
