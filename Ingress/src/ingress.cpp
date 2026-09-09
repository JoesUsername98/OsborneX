#include <Ingress/ingress.hpp>

#include <cassert>
#include <chrono>

namespace OsborneX::Simulation {

Ingress::Ingress(OrderSink& sink)
    : sink_(sink)
{
}

void Ingress::receive(OrderMessage message)
{
#ifndef NDEBUG
    assert_caller_thread();
#endif
    message.ingress_timestamp = current_timestamp();
    message.ingress_sequence = next_sequence_++;
    sink_.route(std::move(message));
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

#ifndef NDEBUG
void Ingress::assert_caller_thread()
{
    // Ingress::receive() has no internal synchronization -- it is only
    // safe to call from a single thread. Catching a violation here in
    // debug builds is cheaper and more direct than diagnosing a corrupted
    // next_sequence_ later.
    const auto id = std::this_thread::get_id();
    if (!caller_thread_.has_value())
        caller_thread_ = id;
    assert(*caller_thread_ == id && "Ingress::receive() called from more than one thread");
}
#endif

} // namespace OsborneX::Simulation
