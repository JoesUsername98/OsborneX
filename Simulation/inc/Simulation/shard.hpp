#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_map>

#include <Orderbook/orderbook.hpp>

#include <Simulation/spsc_queue.hpp>
#include <Simulation/types.hpp>

namespace OsborneX::Simulation {

class MarketDataPublisher;

class Shard
{
public:
    explicit Shard(MarketDataPublisher& publisher, std::size_t queue_capacity = 4096);

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

private:
    void run();
    void process(const OrderMessage& message);
    TopOfBookUpdate make_top_of_book(SymbolId symbol, IngressSequence sequence) const;
    static bool top_of_book_equal(const TopOfBookUpdate& a, const TopOfBookUpdate& b);

    SpscQueue<OrderMessage> queue_;
    std::unordered_map<SymbolId, Orderbook> books_;
    std::unordered_map<SymbolId, TopOfBookUpdate> last_top_;
    MarketDataPublisher& publisher_;
    std::atomic<bool> running_{ false };
    std::thread thread_;
};

} // namespace OsborneX::Simulation
