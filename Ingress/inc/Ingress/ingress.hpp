#pragma once

#include <optional>
#include <thread>

#include <Messages/order_sink.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Simulation {

class Ingress
{
public:
    explicit Ingress(OrderSink& sink);

    void receive(OrderMessage message);
    IngressSequence next_sequence() const;

private:
    Timestamp current_timestamp() const;
#ifndef NDEBUG
    void assert_caller_thread();
#endif

    OrderSink& sink_;
    IngressSequence next_sequence_{ 0 };
#ifndef NDEBUG
    std::optional<std::thread::id> caller_thread_;
#endif
};

} // namespace OsborneX::Simulation
