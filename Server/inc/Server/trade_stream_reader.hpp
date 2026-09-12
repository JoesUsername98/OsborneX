#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include <Messages/trade_execution.hpp>
#include <Queue/ring_buffer.hpp>

namespace OsborneX::Server {

/// @brief Round-robins across registered TradeExecution producer buffers on its own
///        thread, invoking a handler for each delivered trade.
/// @details Deliberately not a change to PubSub::Subscriber/MarketDataPublisher (which
///          are hard-typed to TopOfBookUpdate) -- this mirrors
///          Subscriber::poll_all_sources_once's one-read-per-source-per-pass fairness
///          locally, for TradeExecution, rather than templating a module the project
///          treats as stable and well-tested.
class TradeStreamReader
{
public:
    using Handler = std::function<void(const Simulation::TradeExecution&)>;

    explicit TradeStreamReader(Handler handler = {});

    TradeStreamReader(const TradeStreamReader&) = delete;
    TradeStreamReader& operator=(const TradeStreamReader&) = delete;

    /// Registers a producer's outbound buffer as a source. Must be called before start().
    void register_producer(Queue::RingBuffer<Simulation::TradeExecution>& buffer);

    void start();
    void stop();

private:
    void run();
    bool poll_all_sources_once();

    std::vector<Queue::RingBuffer<Simulation::TradeExecution>::Consumer*> sources_;
    Handler handler_;
    std::atomic<bool> running_{ false };
    std::thread thread_;
};

} // namespace OsborneX::Server
