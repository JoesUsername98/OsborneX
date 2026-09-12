#include <Bot/random_strategy.hpp>

#include <cmath>
#include <mutex>
#include <vector>

#include <Net/order_entry_listener.hpp>
#include <TestSupport/wait_for.hpp>
#include <gtest/gtest.h>

using namespace OsborneX;
using OsborneX::TestSupport::wait_for;

namespace {

Net::OrderEntryClient MakeUnconnectedClient()
{
    // make_random_order() never touches the socket -- an unconnected client is
    // enough for the pure-logic tests below.
    return Net::OrderEntryClient("127.0.0.1", 1);
}

} // namespace

TEST(RandomStrategyTest, UsesDefaultPriceCenterUntilATopOfBookIsObserved)
{
    auto client = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options{};
    options.default_price = 200.0;
    options.price_perturbation_bps = 0.0; // isolate the center, ignore spread
    Bot::RandomStrategy strategy(client, options);

    const auto order = strategy.make_random_order();
    EXPECT_DOUBLE_EQ(order.price, 200.0);
}

TEST(RandomStrategyTest, CentersOnLastKnownMidOnceATopOfBookIsObserved)
{
    auto client = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options{};
    options.default_price = 200.0;
    options.price_perturbation_bps = 0.0;
    Bot::RandomStrategy strategy(client, options);

    strategy.on_top_of_book(Simulation::TopOfBookUpdate{
        .symbol = 1,
        .bid_price = 100.0,
        .bid_quantity = 10,
        .ask_price = 102.0,
        .ask_quantity = 10,
    });

    const auto order = strategy.make_random_order();
    EXPECT_DOUBLE_EQ(order.price, 101.0); // (100 + 102) / 2
}

TEST(RandomStrategyTest, QuantityStaysWithinConfiguredBounds)
{
    auto client = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options{};
    options.min_quantity = 3;
    options.max_quantity = 7;
    Bot::RandomStrategy strategy(client, options);

    for (int i = 0; i < 200; ++i)
    {
        const auto order = strategy.make_random_order();
        EXPECT_GE(order.quantity, options.min_quantity);
        EXPECT_LE(order.quantity, options.max_quantity);
    }
}

TEST(RandomStrategyTest, PriceStaysWithinConfiguredPerturbationBand)
{
    auto client = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options{};
    options.default_price = 100.0;
    options.price_perturbation_bps = 50.0; // +/- 0.5%
    Bot::RandomStrategy strategy(client, options);

    const double lower_bound = 100.0 * (1.0 - 50.0 / 10000.0);
    const double upper_bound = 100.0 * (1.0 + 50.0 / 10000.0);

    for (int i = 0; i < 200; ++i)
    {
        const auto order = strategy.make_random_order();
        EXPECT_GE(order.price, lower_bound);
        EXPECT_LE(order.price, upper_bound);
    }
}

TEST(RandomStrategyTest, BothSidesAppearOverManySamples)
{
    auto client = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options{};
    Bot::RandomStrategy strategy(client, options);

    bool saw_buy = false;
    bool saw_sell = false;
    for (int i = 0; i < 200 && !(saw_buy && saw_sell); ++i)
    {
        const auto order = strategy.make_random_order();
        saw_buy |= (order.side == Simulation::Side::Buy);
        saw_sell |= (order.side == Simulation::Side::Sell);
    }

    EXPECT_TRUE(saw_buy);
    EXPECT_TRUE(saw_sell);
}

TEST(RandomStrategyTest, EachOrderGetsAUniqueMonotonicOrderId)
{
    auto client = MakeUnconnectedClient();
    Bot::RandomStrategy strategy(client);

    const auto first = strategy.make_random_order();
    const auto second = strategy.make_random_order();
    EXPECT_NE(first.order_id, second.order_id);
}

TEST(RandomStrategyTest, OrderIdsAreNamespacedBySourceToAvoidCrossBotCollisions)
{
    // Orderbook::AddOrder keys purely on order_id with no per-source namespacing,
    // so two strategy instances trading the same symbol (as two bots in the demo
    // do) must never generate overlapping ids -- even though each keeps its own
    // order_id counter starting from 1.
    auto client_a = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options_a{};
    options_a.source = 1;
    Bot::RandomStrategy strategy_a(client_a, options_a);

    auto client_b = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options_b{};
    options_b.source = 2;
    Bot::RandomStrategy strategy_b(client_b, options_b);

    for (int i = 0; i < 50; ++i)
    {
        const auto order_a = strategy_a.make_random_order();
        const auto order_b = strategy_b.make_random_order();
        EXPECT_NE(order_a.order_id, order_b.order_id);
    }
}

TEST(RandomStrategyTest, OrdersUseConfiguredSymbolAndGoodTillCancelAdds)
{
    auto client = MakeUnconnectedClient();
    Bot::RandomStrategyOptions options{};
    options.symbol = 42;
    Bot::RandomStrategy strategy(client, options);

    const auto order = strategy.make_random_order();
    EXPECT_EQ(order.symbol, 42u);
    EXPECT_EQ(order.type, Simulation::OrderType::GoodTillCancel);
    EXPECT_EQ(order.action, Simulation::OrderAction::Add);
}

// A real loopback integration test: start() actually opens a timer thread that
// sends over the wire to a real listener, rather than exercising the pure logic.

TEST(RandomStrategyTest, StartedStrategySendsOrdersToARealListener)
{
    std::mutex mutex;
    std::vector<Simulation::OrderMessage> received;
    Net::OrderEntryListener listener(0, [&](Simulation::OrderMessage message) {
        std::lock_guard lock{ mutex };
        received.push_back(message);
    });
    ASSERT_TRUE(listener.start());

    Net::OrderEntryClient client("127.0.0.1", listener.local_port());
    ASSERT_TRUE(client.connect());

    Bot::RandomStrategyOptions options{};
    options.submit_interval = std::chrono::milliseconds{ 10 };
    Bot::RandomStrategy strategy(client, options);
    strategy.start();

    ASSERT_TRUE(wait_for([&] {
        std::lock_guard lock{ mutex };
        return received.size() >= 2;
    }));

    strategy.stop();
    listener.stop();
}
