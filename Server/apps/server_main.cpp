#include <Server/server.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

using namespace OsborneX;

int main(int argc, char** argv)
{
    Server::ServerOptions options{};
    if (argc > 1)
        options.order_entry_port = static_cast<std::uint16_t>(std::stoi(argv[1]));
    if (argc > 2)
        options.market_data_group = argv[2];
    if (argc > 3)
        options.market_data_port = static_cast<std::uint16_t>(std::stoi(argv[3]));

    try
    {
        Server::Server server{ options };
        if (!server.start())
        {
            std::cerr << "Failed to start server: could not bind order-entry port "
                      << options.order_entry_port << " (already in use?)\n";
            return 1;
        }

        std::cout << "OsborneX server listening for orders on port " << options.order_entry_port << '\n';
        std::cout << "Publishing market data to " << options.market_data_group << ':' << options.market_data_port
                  << '\n';
        std::cout << "Press Enter to stop...\n";
        std::cin.get();

        server.stop();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to start server: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
