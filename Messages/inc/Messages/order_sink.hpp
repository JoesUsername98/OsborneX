#pragma once

#include <Messages/types.hpp>

namespace OsborneX::Simulation {

/// @brief Abstract destination for sequenced order flow.
/// @details Lets Ingress depend only on "somewhere to route orders to"
///          rather than concretely on Router, keeping Ingress independent
///          of the Sharding library.
class OrderSink
{
public:
    virtual ~OrderSink() = default;
    virtual void route(OrderMessage message) = 0;
};

} // namespace OsborneX::Simulation
