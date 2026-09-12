#pragma once

#include <thread>

#include <Messages/types.hpp>
#include <Queue/mpsc_queue.hpp>
#include <Simulation/simulation.hpp>

namespace OsborneX::Server {

/// @brief The single thread allowed to call Simulation::submit().
/// @details Simulation::submit -> Ingress::receive -> RingBuffer's producer API are all
///          debug-asserted single-caller-thread, but order-entry connections arrive on
///          many independent socket threads. IngressPump is the one dedicated thread
///          that drains a Queue::MpscQueue<OrderMessage> (fed by any number of producer
///          threads) and is the only thing that ever calls submit().
class IngressPump
{
public:
    IngressPump(Queue::MpscQueue<Simulation::OrderMessage>& inbound, Simulation::Simulation& simulation);

    IngressPump(const IngressPump&) = delete;
    IngressPump& operator=(const IngressPump&) = delete;

    void start();
    void stop();

private:
    void run();

    Queue::MpscQueue<Simulation::OrderMessage>& inbound_;
    Simulation::Simulation& simulation_;
    std::thread thread_;
};

} // namespace OsborneX::Server
