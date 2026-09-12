#include <Server/server.hpp>

#include <utility>

namespace OsborneX::Server {

Server::Server(ServerOptions options)
    : simulation_{ options.simulation_options }
    , inbound_queue_{ options.inbound_queue_capacity }
    , pump_{ inbound_queue_, simulation_ }
    , udp_publisher_{ options.market_data_group, options.market_data_port }
    , broadcaster_{ udp_publisher_, simulation_ }
    , listener_{ options.order_entry_port,
                 [this](Simulation::OrderMessage message) { inbound_queue_.push(std::move(message)); } }
{
}

bool Server::start()
{
    if (running_)
        return true;
    running_ = true;

    simulation_.start();
    broadcaster_.start();
    pump_.start();
    if (!listener_.start())
    {
        pump_.stop();
        simulation_.stop();
        broadcaster_.stop();
        running_ = false;
        return false;
    }
    return true;
}

void Server::stop()
{
    if (!running_)
        return;
    running_ = false;

    // Stop producers before consumers: listener (no new inbound orders) -> pump
    // (drain remaining inbound into simulation) -> simulation (shards finish and
    // do their own final drain, publishing last quotes/fills) -> broadcaster
    // (only now safe to stop reading what simulation just published).
    listener_.stop();
    pump_.stop();
    simulation_.stop();
    broadcaster_.stop();
}

Simulation::Simulation& Server::simulation() noexcept
{
    return simulation_;
}

std::uint16_t Server::order_entry_port() const noexcept
{
    return listener_.local_port();
}

} // namespace OsborneX::Server
