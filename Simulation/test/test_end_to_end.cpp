#include <gtest/gtest.h>

#include <Simulation/simulation.hpp>
#include <Simulation/types.hpp>

#include <chrono>
#include <thread>
#include <unordered_map>

namespace OsborneX::Simulation {
namespace {

OrderMessage MakeAdd(
    SymbolId symbol,
    OrderId orderId,
    Side side,
    Price price,
    Quantity quantity)
{
    return OrderMessage{
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

TEST(EndToEndTest, IngressToSubscriberDeliversTopOfBookUpdates)
{
    Simulation simulation{ 4 };
    Subscriber subscriber;
    simulation.add_subscriber(subscriber);

    subscriber.start();
    simulation.start();

    simulation.submit(MakeAdd(10, 1, Side::Buy, 100.0, 5));
    simulation.submit(MakeAdd(10, 2, Side::Sell, 105.0, 3));
    simulation.submit(MakeAdd(20, 3, Side::Buy, 200.0, 7));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    simulation.stop();
    subscriber.stop();

    const auto events = subscriber.snapshot_events();
    ASSERT_GE(events.size(), 3u);

    std::unordered_map<SymbolId, IngressSequence> last_sequence;
    for (const auto& event : events)
    {
        auto& last = last_sequence[event.symbol];
        EXPECT_GE(event.sequence, last);
        last = event.sequence;
    }

    EXPECT_TRUE(last_sequence.contains(10));
    EXPECT_TRUE(last_sequence.contains(20));
}

TEST(EndToEndTest, IngressAssignsMonotonicSequences)
{
    Simulation simulation{ 2 };
    simulation.start();

    simulation.submit(MakeAdd(1, 1, Side::Buy, 1.0, 1));
    simulation.submit(MakeAdd(1, 2, Side::Buy, 2.0, 1));
    simulation.submit(MakeAdd(1, 3, Side::Buy, 3.0, 1));

    EXPECT_EQ(simulation.ingress().next_sequence(), 3u);

    simulation.stop();
}

} // namespace
} // namespace OsborneX::Simulation
