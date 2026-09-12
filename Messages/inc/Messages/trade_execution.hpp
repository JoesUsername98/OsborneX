#pragma once

#include <Messages/types.hpp>

namespace OsborneX::Simulation {

/// @brief A single trade print resulting from a crossing order.
/// @details Distinct from TopOfBookUpdate (book state) rather than folded into it,
///          since multiple trades can occur between book-state changes and a
///          best-effort/overwrite-style stream must not collapse them into one
///          "last trade" field the way it collapses top-of-book to "latest only".
///          Bid/ask prices are captured independently (not assumed equal) since a
///          crossing order fills against a resting order at the resting order's own
///          price, per Orderbook::MatchOrders.
struct TradeExecution
{
    SymbolId symbol{};
    IngressSequence sequence{};
    Timestamp timestamp{};
    OrderId bid_order_id{};
    Price bid_price{};
    OrderId ask_order_id{};
    Price ask_price{};
    Quantity quantity{};
};

} // namespace OsborneX::Simulation
