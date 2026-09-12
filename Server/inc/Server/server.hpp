#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <Messages/types.hpp>
#include <Net/market_data_multicast.hpp>
#include <Net/order_entry_listener.hpp>
#include <Queue/mpsc_queue.hpp>
#include <Server/ingress_pump.hpp>
#include <Server/market_data_broadcaster.hpp>
#include <Simulation/simulation.hpp>

namespace OsborneX::Server {

struct ServerOptions
{
    std::uint16_t order_entry_port{ 9001 };
    std::string market_data_group{ "239.1.1.1" };
    std::uint16_t market_data_port{ 9002 };
    Simulation::SimulationOptions simulation_options{};
    std::size_t inbound_queue_capacity{ 4096 };
};

/// @brief Wraps Simulation with a TCP order-entry endpoint and a UDP market-data feed.
/// @details Wiring order matters: start() brings the simulation and broadcaster up
///          before accepting any connections, so no order can be submitted before
///          something is listening for the market data it produces; stop() reverses
///          that, stopping the listener first so no new order arrives mid-shutdown.
class Server
{
public:
    explicit Server(ServerOptions options = {});

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /// @return false if the order-entry listener failed to bind (e.g. port already
    ///         in use) -- simulation/broadcaster/pump are rolled back before
    ///         returning so a failed start() never leaves the server half-running.
    bool start();
    void stop();

    Simulation::Simulation& simulation() noexcept;

    /// The TCP port actually bound, once start() has succeeded -- resolves a
    /// configured port of 0 ("let the OS choose") to its real value.
    std::uint16_t order_entry_port() const noexcept;

private:
    Simulation::Simulation simulation_;
    Queue::MpscQueue<Simulation::OrderMessage> inbound_queue_;
    IngressPump pump_;
    Net::MarketDataPublisherUdp udp_publisher_;
    MarketDataBroadcaster broadcaster_;
    Net::OrderEntryListener listener_;
    bool running_{ false };
};

} // namespace OsborneX::Server
