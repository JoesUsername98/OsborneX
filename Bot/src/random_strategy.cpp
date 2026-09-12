#include <Bot/random_strategy.hpp>

#include <algorithm>

namespace OsborneX::Bot {

RandomStrategy::RandomStrategy(Net::OrderEntryClient& order_client, RandomStrategyOptions options)
    : order_client_{ order_client }
    , options_{ options }
    , rng_{ options.random_seed }
{
}

void RandomStrategy::on_top_of_book(const Simulation::TopOfBookUpdate& update)
{
    std::lock_guard lock{ top_mutex_ };
    last_known_tops_[update.symbol] = update;
}

void RandomStrategy::start()
{
    if (running_.exchange(true))
        return;
    thread_ = std::thread(&RandomStrategy::run, this);
}

void RandomStrategy::stop()
{
    if (!running_.exchange(false))
        return;
    if (thread_.joinable())
        thread_.join();
}

Simulation::OrderMessage RandomStrategy::make_random_order()
{
    std::uniform_int_distribution<std::size_t> symbol_dist(0, options_.symbols.size() - 1);
    const Simulation::SymbolId symbol = options_.symbols[symbol_dist(rng_)];

    Simulation::Price center = options_.default_price;
    {
        std::lock_guard lock{ top_mutex_ };
        const auto it = last_known_tops_.find(symbol);
        if (it != last_known_tops_.end())
        {
            const auto& top = it->second;
            if (Simulation::HasBid(top) && Simulation::HasAsk(top))
                center = (top.bid_price + top.ask_price) / 2.0;
            else if (Simulation::HasBid(top))
                center = top.bid_price;
            else if (Simulation::HasAsk(top))
                center = top.ask_price;
        }
    }

    std::uniform_int_distribution<int> side_dist(0, 1);
    const auto side = side_dist(rng_) == 0 ? Simulation::Side::Buy : Simulation::Side::Sell;

    std::uniform_real_distribution<double> perturbation_dist(
        -options_.price_perturbation_bps, options_.price_perturbation_bps);
    const double perturbation_fraction = perturbation_dist(rng_) / 10000.0;
    const Simulation::Price price = center * (1.0 + perturbation_fraction);

    std::uniform_int_distribution<Simulation::Quantity> quantity_dist(options_.min_quantity, options_.max_quantity);
    const Simulation::Quantity quantity = quantity_dist(rng_);

    // Orderbook::orders_ is keyed on order_id alone with no per-source namespacing
    // (see Orderbook::AddOrder), so two strategy instances sharing a symbol must
    // never hand out the same id. Reserving the top 16 bits for `source` (and
    // relying on the caller to give each bot a distinct one) guarantees disjoint
    // ranges instead of relying on the local counters merely being "unlikely" to
    // collide.
    constexpr Simulation::OrderId local_id_bits = 48;
    constexpr Simulation::OrderId local_id_mask = (Simulation::OrderId{ 1 } << local_id_bits) - 1;
    const Simulation::OrderId local_id = next_order_id_.fetch_add(1, std::memory_order_relaxed) & local_id_mask;
    const Simulation::OrderId order_id = (static_cast<Simulation::OrderId>(options_.source) << local_id_bits) | local_id;

    return Simulation::OrderMessage{
        .source = options_.source,
        .symbol = symbol,
        .order_id = order_id,
        .side = side,
        .price = price,
        .quantity = quantity,
        .type = Simulation::OrderType::GoodTillCancel,
        .action = Simulation::OrderAction::Add,
    };
}

void RandomStrategy::run()
{
    constexpr auto poll_interval = std::chrono::milliseconds{ 20 };

    while (running_.load(std::memory_order_acquire))
    {
        order_client_.send(make_random_order());

        // Sleep in small increments so stop() is noticed promptly instead of
        // blocking for up to the full (potentially much longer) submit_interval.
        auto remaining = options_.submit_interval;
        while (remaining.count() > 0 && running_.load(std::memory_order_acquire))
        {
            const auto step = std::min(remaining, poll_interval);
            std::this_thread::sleep_for(step);
            remaining -= step;
        }
    }
}

} // namespace OsborneX::Bot
