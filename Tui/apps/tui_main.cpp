#include <Tui/market_view_model.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <Net/market_data_multicast.hpp>
#include <Net/order_entry_client.hpp>

using namespace OsborneX;
using namespace ftxui;

namespace {

std::string FormatPrice(Simulation::Price price)
{
    if (std::isnan(price))
        return "--";
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << price;
    return stream.str();
}

Element RenderBooks(const std::vector<Simulation::TopOfBookUpdate>& books)
{
    Elements rows;
    rows.push_back(hbox({
                       text("Symbol") | size(WIDTH, EQUAL, 10),
                       text("Bid Qty") | size(WIDTH, EQUAL, 10),
                       text("Bid") | size(WIDTH, EQUAL, 10),
                       text("Ask") | size(WIDTH, EQUAL, 10),
                       text("Ask Qty") | size(WIDTH, EQUAL, 10),
                   }) |
                   bold);
    rows.push_back(separator());

    for (const auto& book : books)
    {
        rows.push_back(hbox({
            text(std::to_string(book.symbol)) | size(WIDTH, EQUAL, 10),
            text(Simulation::HasBid(book) ? std::to_string(book.bid_quantity) : "--") |
                size(WIDTH, EQUAL, 10) | color(Color::Green),
            text(FormatPrice(book.bid_price)) | size(WIDTH, EQUAL, 10) | color(Color::Green),
            text(FormatPrice(book.ask_price)) | size(WIDTH, EQUAL, 10) | color(Color::Red),
            text(Simulation::HasAsk(book) ? std::to_string(book.ask_quantity) : "--") |
                size(WIDTH, EQUAL, 10) | color(Color::Red),
        }));
    }

    if (books.empty())
        rows.push_back(text("(no market data yet)") | dim);

    return window(text(" Order Book "), vbox(rows));
}

Element RenderTrades(const std::vector<Simulation::TradeExecution>& trades)
{
    Elements rows;
    rows.push_back(hbox({
                       text("Symbol") | size(WIDTH, EQUAL, 10),
                       text("Price") | size(WIDTH, EQUAL, 10),
                       text("Qty") | size(WIDTH, EQUAL, 10),
                   }) |
                   bold);
    rows.push_back(separator());

    for (const auto& trade : trades)
    {
        rows.push_back(hbox({
            text(std::to_string(trade.symbol)) | size(WIDTH, EQUAL, 10),
            text(FormatPrice(trade.bid_price)) | size(WIDTH, EQUAL, 10),
            text(std::to_string(trade.quantity)) | size(WIDTH, EQUAL, 10),
        }));
    }

    if (trades.empty())
        rows.push_back(text("(no trades yet)") | dim);

    return window(text(" Trade Tape "), vbox(rows) | frame | size(HEIGHT, LESS_THAN, 20));
}

} // namespace

int main(int argc, char** argv)
{
    std::string market_data_group = "239.1.1.1";
    std::uint16_t market_data_port = 9002;
    std::string order_entry_host = "127.0.0.1";
    std::uint16_t order_entry_port = 9001;

    if (argc > 1)
        market_data_group = argv[1];
    if (argc > 2)
        market_data_port = static_cast<std::uint16_t>(std::stoi(argv[2]));
    if (argc > 3)
        order_entry_host = argv[3];
    if (argc > 4)
        order_entry_port = static_cast<std::uint16_t>(std::stoi(argv[4]));

    Tui::MarketViewModel view_model;

    Net::MarketDataListener market_data(
        market_data_group, market_data_port,
        [&](const Simulation::TopOfBookUpdate& update) { view_model.on_top_of_book(update); },
        [&](const Simulation::TradeExecution& trade) { view_model.on_trade(trade); });
    market_data.start();

    // Constructed, connected, and never sent from in this passive-viewer iteration.
    // Keeping this connection already wired up is what makes a future interactive
    // TUI (letting the viewer submit orders) a small addition -- wire an input
    // component to order_client.send() -- rather than a re-plumb.
    Net::OrderEntryClient order_client(order_entry_host, order_entry_port);
    order_client.connect();

    auto screen = ScreenInteractive::Fullscreen();

    std::atomic<bool> refreshing{ true };
    std::thread refresh_thread([&] {
        while (refreshing.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            screen.PostEvent(Event::Custom);
        }
    });

    auto renderer = Renderer([&] {
        return vbox({
                   text("OsborneX -- live market data") | bold | center,
                   separator(),
                   RenderBooks(view_model.snapshot_books()),
                   RenderTrades(view_model.snapshot_trades()),
               }) |
               border;
    });

    screen.Loop(renderer);

    refreshing.store(false, std::memory_order_release);
    refresh_thread.join();

    market_data.stop();
    order_client.disconnect();
    return 0;
}
