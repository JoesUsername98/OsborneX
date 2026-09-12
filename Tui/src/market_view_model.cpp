#include <Tui/market_view_model.hpp>

namespace OsborneX::Tui {

MarketViewModel::MarketViewModel(std::size_t max_trade_tape_size)
    : max_trade_tape_size_{ max_trade_tape_size == 0 ? std::size_t{ 1 } : max_trade_tape_size }
{
}

void MarketViewModel::on_top_of_book(const Simulation::TopOfBookUpdate& update)
{
    std::lock_guard lock{ mutex_ };
    books_[update.symbol] = update;
}

void MarketViewModel::on_trade(const Simulation::TradeExecution& trade)
{
    std::lock_guard lock{ mutex_ };
    trades_.push_front(trade);
    while (trades_.size() > max_trade_tape_size_)
        trades_.pop_back();
}

std::vector<Simulation::TopOfBookUpdate> MarketViewModel::snapshot_books() const
{
    std::lock_guard lock{ mutex_ };
    std::vector<Simulation::TopOfBookUpdate> result;
    result.reserve(books_.size());
    for (const auto& [symbol, update] : books_)
        result.push_back(update);
    return result;
}

std::vector<Simulation::TradeExecution> MarketViewModel::snapshot_trades() const
{
    std::lock_guard lock{ mutex_ };
    return std::vector<Simulation::TradeExecution>(trades_.begin(), trades_.end());
}

} // namespace OsborneX::Tui
