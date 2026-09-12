#include <Server/ingress_pump.hpp>

namespace OsborneX::Server {

IngressPump::IngressPump(Queue::MpscQueue<Simulation::OrderMessage>& inbound, Simulation::Simulation& simulation)
    : inbound_{ inbound }
    , simulation_{ simulation }
{
}

void IngressPump::start()
{
    thread_ = std::thread(&IngressPump::run, this);
}

void IngressPump::stop()
{
    // Producers must have stopped pushing before this is called (same hand-off
    // discipline as MpscQueue::close()'s documented contract) -- close() wakes
    // run()'s blocked pop_blocking() and lets it drain whatever remains.
    inbound_.close();
    if (thread_.joinable())
        thread_.join();
}

void IngressPump::run()
{
    while (auto message = inbound_.pop_blocking())
        simulation_.submit(std::move(*message));
}

} // namespace OsborneX::Server
