#include <Net/market_data_multicast.hpp>

#include <mutex>
#include <optional>
#include <vector>

#include <TestSupport/wait_for.hpp>
#include <gtest/gtest.h>

using namespace OsborneX;
using OsborneX::TestSupport::wait_for;

namespace {

// A fixed administratively-scoped multicast group/port pair, distinct per test to
// avoid cross-test interference if datagrams from one test's socket teardown are
// still in flight when the next test's listener joins the group.

template <typename T>
class ThreadSafeLog
{
public:
    void add(T value)
    {
        std::lock_guard lock{ mutex_ };
        values_.push_back(std::move(value));
    }

    std::vector<T> snapshot() const
    {
        std::lock_guard lock{ mutex_ };
        return values_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<T> values_;
};

} // namespace

TEST(MulticastLoopbackTest, PublisherSendTopOfBookIsDeliveredToListener)
{
    ThreadSafeLog<Simulation::TopOfBookUpdate> received;
    Net::MarketDataListener listener(
        "239.255.10.1", 23981,
        [&](const Simulation::TopOfBookUpdate& update) { received.add(update); },
        [](const Simulation::TradeExecution&) {});
    ASSERT_TRUE(listener.start());

    Net::MarketDataPublisherUdp publisher("239.255.10.1", 23981);

    const Simulation::TopOfBookUpdate update{
        .symbol = 5,
        .bid_price = 100.0,
        .bid_quantity = 10,
        .ask_price = 101.0,
        .ask_quantity = 20,
        .sequence = 3,
    };

    // A multicast join can take a beat to become effective, and UDP is inherently
    // best-effort -- keep sending until wait_for observes delivery rather than
    // asserting on a single fire-and-forget send.
    bool delivered = wait_for([&] {
        publisher.send_top_of_book(update);
        return !received.snapshot().empty();
    });
    ASSERT_TRUE(delivered);

    const auto updates = received.snapshot();
    EXPECT_EQ(updates.front().symbol, update.symbol);
    EXPECT_DOUBLE_EQ(updates.front().bid_price, update.bid_price);
    EXPECT_EQ(updates.front().bid_quantity, update.bid_quantity);

    listener.stop();
}

TEST(MulticastLoopbackTest, PublisherSendTradeIsDeliveredToListener)
{
    ThreadSafeLog<Simulation::TradeExecution> received;
    Net::MarketDataListener listener(
        "239.255.10.2", 23982,
        [](const Simulation::TopOfBookUpdate&) {},
        [&](const Simulation::TradeExecution& trade) { received.add(trade); });
    ASSERT_TRUE(listener.start());

    Net::MarketDataPublisherUdp publisher("239.255.10.2", 23982);

    const Simulation::TradeExecution trade{
        .symbol = 9,
        .sequence = 1,
        .timestamp = 2,
        .bid_order_id = 1,
        .bid_price = 50.0,
        .ask_order_id = 2,
        .ask_price = 50.0,
        .quantity = 4,
    };

    bool delivered = wait_for([&] {
        publisher.send_trade(trade);
        return !received.snapshot().empty();
    });
    ASSERT_TRUE(delivered);

    const auto trades = received.snapshot();
    EXPECT_EQ(trades.front().symbol, trade.symbol);
    EXPECT_EQ(trades.front().bid_order_id, trade.bid_order_id);
    EXPECT_EQ(trades.front().ask_order_id, trade.ask_order_id);
    EXPECT_EQ(trades.front().quantity, trade.quantity);

    listener.stop();
}
