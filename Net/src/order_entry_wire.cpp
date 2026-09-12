#include <Net/wire/order_entry_wire.hpp>

namespace OsborneX::Net::wire {

OrderEntryWire encode(const Simulation::OrderMessage& message)
{
    return OrderEntryWire{
        .source_timestamp = message.source_timestamp,
        .source = message.source,
        .symbol = message.symbol,
        .order_id = message.order_id,
        .side = static_cast<std::uint8_t>(message.side),
        .order_type = static_cast<std::uint8_t>(message.type),
        .action = static_cast<std::uint8_t>(message.action),
        .price = message.price,
        .quantity = message.quantity,
    };
}

Simulation::OrderMessage decode(const OrderEntryWire& wire)
{
    Simulation::OrderMessage message{};
    message.source_timestamp = wire.source_timestamp;
    message.source = wire.source;
    message.symbol = wire.symbol;
    message.order_id = wire.order_id;
    message.side = static_cast<Simulation::Side>(wire.side);
    message.type = static_cast<Simulation::OrderType>(wire.order_type);
    message.action = static_cast<Simulation::OrderAction>(wire.action);
    message.price = wire.price;
    message.quantity = wire.quantity;
    return message;
}

} // namespace OsborneX::Net::wire
