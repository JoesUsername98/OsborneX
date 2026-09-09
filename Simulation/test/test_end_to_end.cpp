#include <gtest/gtest.h>

#include <Simulation/simulation.hpp>
#include <Messages/types.hpp>
#include <TestSupport/wait_for.hpp>

#include <unordered_map>

namespace OsborneX::Simulation {
namespace {

using OsborneX::TestSupport::wait_for;

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

    ASSERT_TRUE(wait_for([&] { return subscriber.snapshot_events().size() >= 3; }));
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
    EXPECT_EQ(subscriber.dropped_count(), 0u);
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
