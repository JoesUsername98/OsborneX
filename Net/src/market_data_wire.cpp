#include <Net/wire/market_data_wire.hpp>

namespace OsborneX::Net::wire {

TopOfBookUpdateWire encode(const Simulation::TopOfBookUpdate& update)
{
    return TopOfBookUpdateWire{
        .symbol = update.symbol,
        .bid_price = update.bid_price,
        .bid_quantity = update.bid_quantity,
        .ask_price = update.ask_price,
        .ask_quantity = update.ask_quantity,
        .sequence = update.sequence,
    };
}

Simulation::TopOfBookUpdate decode(const TopOfBookUpdateWire& wire)
{
    return Simulation::TopOfBookUpdate{
        .symbol = wire.symbol,
        .bid_price = wire.bid_price,
        .bid_quantity = wire.bid_quantity,
        .ask_price = wire.ask_price,
        .ask_quantity = wire.ask_quantity,
        .sequence = wire.sequence,
    };
}

TradeExecutionWire encode(const Simulation::TradeExecution& trade)
{
    return TradeExecutionWire{
        .symbol = trade.symbol,
        .sequence = trade.sequence,
        .timestamp = trade.timestamp,
        .bid_order_id = trade.bid_order_id,
        .bid_price = trade.bid_price,
        .ask_order_id = trade.ask_order_id,
        .ask_price = trade.ask_price,
        .quantity = trade.quantity,
    };
}

Simulation::TradeExecution decode(const TradeExecutionWire& wire)
{
    return Simulation::TradeExecution{
        .symbol = wire.symbol,
        .sequence = wire.sequence,
        .timestamp = wire.timestamp,
        .bid_order_id = wire.bid_order_id,
        .bid_price = wire.bid_price,
        .ask_order_id = wire.ask_order_id,
        .ask_price = wire.ask_price,
        .quantity = wire.quantity,
    };
}

} // namespace OsborneX::Net::wire
