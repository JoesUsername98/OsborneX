#pragma once

#include <cstdint>

#include <Messages/trade_execution.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Net::wire {

/// @brief Explicit fixed-width wire representations of TopOfBookUpdate/TradeExecution.
///        Same rationale as OrderEntryWire: explicit encode/decode instead of a
///        reinterpret_cast of the in-memory structs.
#pragma pack(push, 1)
struct TopOfBookUpdateWire
{
    std::uint32_t symbol{};
    double bid_price{};
    std::uint32_t bid_quantity{};
    double ask_price{};
    std::uint32_t ask_quantity{};
    std::uint64_t sequence{};
};

struct TradeExecutionWire
{
    std::uint32_t symbol{};
    std::uint64_t sequence{};
    std::uint64_t timestamp{};
    std::uint64_t bid_order_id{};
    double bid_price{};
    std::uint64_t ask_order_id{};
    double ask_price{};
    std::uint32_t quantity{};
};
#pragma pack(pop)

TopOfBookUpdateWire encode(const Simulation::TopOfBookUpdate& update);
Simulation::TopOfBookUpdate decode(const TopOfBookUpdateWire& wire);

TradeExecutionWire encode(const Simulation::TradeExecution& trade);
Simulation::TradeExecution decode(const TradeExecutionWire& wire);

} // namespace OsborneX::Net::wire
