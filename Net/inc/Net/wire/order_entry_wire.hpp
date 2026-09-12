#pragma once

#include <cstdint>

#include <Messages/types.hpp>

namespace OsborneX::Net::wire {

/// @brief Explicit fixed-width wire representation of OrderMessage.
/// @details Deliberately not a reinterpret_cast of the in-memory OrderMessage: that
///          struct's Side/OrderType enum classes don't pin an underlying type, and its
///          layout isn't guaranteed identical across MSVC/GCC. Encoding to/from this
///          struct makes both explicit. ingress_timestamp/ingress_sequence are omitted
///          on purpose -- those are stamped by the server's Ingress on receipt, never
///          supplied by the client.
#pragma pack(push, 1)
struct OrderEntryWire
{
    std::uint64_t source_timestamp{};
    std::uint16_t source{};
    std::uint32_t symbol{};
    std::uint64_t order_id{};
    std::uint8_t side{};
    std::uint8_t order_type{};
    std::uint8_t action{};
    double price{};
    std::uint32_t quantity{};
};
#pragma pack(pop)

OrderEntryWire encode(const Simulation::OrderMessage& message);
Simulation::OrderMessage decode(const OrderEntryWire& wire);

} // namespace OsborneX::Net::wire
