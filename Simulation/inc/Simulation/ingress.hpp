#pragma once

#include <Simulation/router.hpp>
#include <Simulation/types.hpp>

namespace OsborneX::Simulation {

class Ingress
{
public:
    explicit Ingress(Router& router);

    void receive(OrderMessage message);
    IngressSequence next_sequence() const;

private:
    Timestamp current_timestamp() const;

    Router& router_;
    IngressSequence next_sequence_{ 0 };
};

} // namespace OsborneX::Simulation
