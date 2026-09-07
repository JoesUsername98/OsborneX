#pragma once

#include <cstdint>
#include <limits>

#include <Orderbook/domain_types.hpp>
#include <Orderbook/order_type.hpp>
#include <Orderbook/side.hpp>

namespace OsborneX::Simulation {

using SymbolId = std::uint32_t;
using SourceId = std::uint16_t;
using Timestamp = std::uint64_t;
using IngressSequence = std::uint64_t;

using Price = OsborneX::Price;
using Quantity = OsborneX::Quantity;
using OrderId = OsborneX::OrderId;
using Side = OsborneX::Side;
using OrderType = OsborneX::OrderType;

enum class OrderAction : std::uint8_t
{
    Add,
    Cancel,
    Modify
};

struct OrderMessage
{
    Timestamp source_timestamp{};
    Timestamp ingress_timestamp{};
    IngressSequence ingress_sequence{};
    SourceId source{};
    SymbolId symbol{};
    OrderId order_id{};
    Side side{};
    Price price{};
    Quantity quantity{};
    OrderType type{};
    OrderAction action{ OrderAction::Add };
};

struct TopOfBookUpdate
{
    SymbolId symbol{};
    Price bid_price{ std::numeric_limits<Price>::quiet_NaN() };
    Quantity bid_quantity{};
    Price ask_price{ std::numeric_limits<Price>::quiet_NaN() };
    Quantity ask_quantity{};
    IngressSequence sequence{};
};

inline bool HasBid(const TopOfBookUpdate& update)
{
    return update.bid_quantity > 0;
}

inline bool HasAsk(const TopOfBookUpdate& update)
{
    return update.ask_quantity > 0;
}

} // namespace OsborneX::Simulation
