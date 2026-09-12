#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

#include <Messages/trade_execution.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Tui {

/// @brief Pure, terminal-independent market state, fed by a Net::MarketDataListener's
///        handlers and read by the FTXUI render loop.
/// @details Deliberately has no FTXUI dependency, so it's unit-testable without a
///          terminal -- all FTXUI-specific code lives only in apps/tui_main.cpp.
class MarketViewModel
{
public:
    explicit MarketViewModel(std::size_t max_trade_tape_size = 100);

    void on_top_of_book(const Simulation::TopOfBookUpdate& update);
    void on_trade(const Simulation::TradeExecution& trade);

    /// Latest top-of-book per symbol, ordered by symbol.
    std::vector<Simulation::TopOfBookUpdate> snapshot_books() const;

    /// Most recently received trade first, bounded to max_trade_tape_size.
    std::vector<Simulation::TradeExecution> snapshot_trades() const;

private:
    mutable std::mutex mutex_;
    std::map<Simulation::SymbolId, Simulation::TopOfBookUpdate> books_;
    std::deque<Simulation::TradeExecution> trades_;
    std::size_t max_trade_tape_size_;
};

} // namespace OsborneX::Tui
