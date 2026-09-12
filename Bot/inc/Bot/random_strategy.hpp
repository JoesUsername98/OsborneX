#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#include <Messages/types.hpp>
#include <Net/order_entry_client.hpp>

namespace OsborneX::Bot {

struct RandomStrategyOptions
{
    /// Each generated order draws its symbol uniformly at random from this set --
    /// must be non-empty.
    std::vector<Simulation::SymbolId> symbols{ 1 };
    Simulation::SourceId source{ 1 };
    std::chrono::milliseconds submit_interval{ 200 };
    /// Used as the order's price center until a real top-of-book is observed.
    Simulation::Price default_price{ 100.0 };
    Simulation::Quantity min_quantity{ 1 };
    Simulation::Quantity max_quantity{ 20 };
    /// Random price spread applied around the price center, in basis points.
    double price_perturbation_bps{ 25.0 };
    unsigned random_seed{ std::random_device{}() };
};

/// @brief Deliberately simple randomized buy/sell strategy: fire-and-forget
///        GoodTillCancel adds only, no cancels, no tracking of its own resting
///        orders -- a reasonable v1 default the user can extend later (own-order
///        tracking, cancels/modifies, a less naive price model).
class RandomStrategy
{
public:
    explicit RandomStrategy(Net::OrderEntryClient& order_client, RandomStrategyOptions options = {});

    RandomStrategy(const RandomStrategy&) = delete;
    RandomStrategy& operator=(const RandomStrategy&) = delete;

    /// Feeds the latest observed top-of-book for its symbol so subsequent random
    /// orders on that symbol are priced around the live market instead of always
    /// falling back to options.default_price. Safe to call from any thread (e.g. a
    /// Net::MarketDataListener's handler).
    void on_top_of_book(const Simulation::TopOfBookUpdate& update);

    void start();
    void stop();

    /// Builds one randomized order without sending it -- the pure, deterministic
    /// (given a fixed random_seed) piece of the strategy, exposed for unit testing
    /// without needing a real socket.
    Simulation::OrderMessage make_random_order();

private:
    void run();

    Net::OrderEntryClient& order_client_;
    RandomStrategyOptions options_;

    std::mutex top_mutex_;
    std::unordered_map<Simulation::SymbolId, Simulation::TopOfBookUpdate> last_known_tops_;

    std::mt19937 rng_;
    std::atomic<Simulation::OrderId> next_order_id_{ 1 };

    std::atomic<bool> running_{ false };
    std::thread thread_;
};

} // namespace OsborneX::Bot
