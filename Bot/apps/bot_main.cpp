#include <Bot/random_strategy.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include <Net/market_data_multicast.hpp>
#include <Net/order_entry_client.hpp>

using namespace OsborneX;

int main(int argc, char** argv)
{
    std::string server_host = "127.0.0.1";
    std::uint16_t order_entry_port = 9001;
    std::string market_data_group = "239.1.1.1";
    std::uint16_t market_data_port = 9002;
    Simulation::SymbolId symbol = 1;
    Simulation::SourceId source = 1;

    if (argc > 1)
        server_host = argv[1];
    if (argc > 2)
        order_entry_port = static_cast<std::uint16_t>(std::stoi(argv[2]));
    if (argc > 3)
        market_data_group = argv[3];
    if (argc > 4)
        market_data_port = static_cast<std::uint16_t>(std::stoi(argv[4]));
    if (argc > 5)
        symbol = static_cast<Simulation::SymbolId>(std::stoul(argv[5]));
    if (argc > 6)
        source = static_cast<Simulation::SourceId>(std::stoul(argv[6]));

    Net::OrderEntryClient order_client(server_host, order_entry_port);

    // Retry for a while rather than failing on the first attempt -- when a bot is
    // launched alongside the server (e.g. via a debugger compound launch), there is
    // no guarantee the server's listener is already up yet.
    constexpr int max_attempts = 20;
    constexpr auto retry_delay = std::chrono::milliseconds{ 500 };
    bool connected = false;
    for (int attempt = 0; attempt < max_attempts && !connected; ++attempt)
    {
        connected = order_client.connect();
        if (!connected)
        {
            std::cout << "Waiting for server at " << server_host << ':' << order_entry_port << "...\n";
            std::this_thread::sleep_for(retry_delay);
        }
    }
    if (!connected)
    {
        std::cerr << "Failed to connect to server at " << server_host << ':' << order_entry_port
                  << " after " << max_attempts << " attempts\n";
        return 1;
    }

    Bot::RandomStrategyOptions options{};
    options.symbol = symbol;
    options.source = source;

    Bot::RandomStrategy strategy(order_client, options);

    Net::MarketDataListener market_data(
        market_data_group, market_data_port,
        [&](const Simulation::TopOfBookUpdate& update) {
            if (update.symbol == symbol)
                strategy.on_top_of_book(update);
        },
        [](const Simulation::TradeExecution&) {});
    market_data.start();

    strategy.start();

    std::cout << "OsborneX bot trading symbol " << symbol << " against " << server_host << ':' << order_entry_port
              << ". Press Enter to stop...\n";
    std::cin.get();

    strategy.stop();
    market_data.stop();
    order_client.disconnect();
    return 0;
}
