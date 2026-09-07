#include <Simulation/ingress.hpp>

#include <chrono>

namespace OsborneX::Simulation {

Ingress::Ingress(Router& router)
    : router_(router)
{
}

void Ingress::receive(OrderMessage message)
{
    message.ingress_timestamp = current_timestamp();
    message.ingress_sequence = next_sequence_++;
    router_.route(std::move(message));
}

IngressSequence Ingress::next_sequence() const
{
    return next_sequence_;
}

Timestamp Ingress::current_timestamp() const
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<Timestamp>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace OsborneX::Simulation
