#include <Simulation/simulation.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace OsborneX::Simulation;

namespace {

OrderMessage MakeAdd(
    SymbolId symbol,
    OrderId orderId,
    Side side,
    Price price,
    Quantity quantity)
{
    return OrderMessage{
        .source_timestamp = 0,
        .source = 1,
        .symbol = symbol,
        .order_id = orderId,
        .side = side,
        .price = price,
        .quantity = quantity,
        .type = OrderType::GoodTillCancel,
        .action = OrderAction::Add,
    };
}

} // namespace

int main()
{
    Simulation simulation{ 4 };
    Subscriber subscriber{ 256 };
    simulation.add_subscriber(subscriber);

    subscriber.start();
    simulation.start();

    simulation.submit(MakeAdd(1, 1, Side::Buy, 100.0, 10));
    simulation.submit(MakeAdd(1, 2, Side::Sell, 101.0, 5));
    simulation.submit(MakeAdd(2, 3, Side::Buy, 50.0, 20));
    simulation.submit(MakeAdd(2, 4, Side::Sell, 51.0, 8));
    simulation.submit(MakeAdd(3, 5, Side::Buy, 10.0, 100));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    simulation.stop();
    subscriber.stop();

    const auto events = subscriber.snapshot_events();
    std::cout << "Top-of-book updates received: " << events.size() << '\n';
    std::cout << "Updates dropped: " << subscriber.dropped_count() << '\n';

    for (const auto& event : events)
    {
        std::cout << "symbol=" << event.symbol
                  << " seq=" << event.sequence
                  << " bid=" << event.bid_quantity << '@' << event.bid_price
                  << " ask=" << event.ask_quantity << '@' << event.ask_price
                  << '\n';
    }

    return events.empty() ? 1 : 0;
}
