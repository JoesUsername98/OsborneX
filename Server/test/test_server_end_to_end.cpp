#include <Server/server.hpp>

#include <mutex>
#include <vector>

#include <Net/market_data_multicast.hpp>
#include <Net/order_entry_client.hpp>
#include <TestSupport/wait_for.hpp>
#include <gtest/gtest.h>

using namespace OsborneX;
using OsborneX::TestSupport::wait_for;

namespace {

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

TEST(ServerEndToEndTest, CrossingOrdersFromTcpClientsProduceTopOfBookAndTradeOverMulticast)
{
    Server::ServerOptions options{};
    options.order_entry_port = 0; // let the OS choose; queried below once bound
    options.market_data_group = "239.255.20.1";
    options.market_data_port = 23990;
    options.simulation_options.shard_count = 2;

    Server::Server server{ options };
    server.start();

    ThreadSafeLog<Simulation::TopOfBookUpdate> top_of_book_log;
    ThreadSafeLog<Simulation::TradeExecution> trade_log;
    Net::MarketDataListener market_data(
        options.market_data_group, options.market_data_port,
        [&](const Simulation::TopOfBookUpdate& update) { top_of_book_log.add(update); },
        [&](const Simulation::TradeExecution& trade) { trade_log.add(trade); });
    ASSERT_TRUE(market_data.start());

    Net::OrderEntryClient buyer("127.0.0.1", server.order_entry_port());
    Net::OrderEntryClient seller("127.0.0.1", server.order_entry_port());
    ASSERT_TRUE(buyer.connect());
    ASSERT_TRUE(seller.connect());

    ASSERT_TRUE(buyer.send(Simulation::OrderMessage{
        .symbol = 1,
        .order_id = 1,
        .side = Simulation::Side::Buy,
        .price = 100.0,
        .quantity = 10,
        .type = Simulation::OrderType::GoodTillCancel,
        .action = Simulation::OrderAction::Add,
    }));
    ASSERT_TRUE(seller.send(Simulation::OrderMessage{
        .symbol = 1,
        .order_id = 2,
        .side = Simulation::Side::Sell,
        .price = 100.0,
        .quantity = 10,
        .type = Simulation::OrderType::GoodTillCancel,
        .action = Simulation::OrderAction::Add,
    }));

    ASSERT_TRUE(wait_for([&] { return !trade_log.snapshot().empty(); }));
    ASSERT_TRUE(wait_for([&] { return !top_of_book_log.snapshot().empty(); }));

    const auto trades = trade_log.snapshot();
    EXPECT_EQ(trades.front().symbol, 1u);
    EXPECT_EQ(trades.front().bid_order_id, 1u);
    EXPECT_EQ(trades.front().ask_order_id, 2u);
    EXPECT_EQ(trades.front().quantity, 10u);

    bool saw_symbol_one = false;
    for (const auto& update : top_of_book_log.snapshot())
        saw_symbol_one |= (update.symbol == 1u);
    EXPECT_TRUE(saw_symbol_one);

    buyer.disconnect();
    seller.disconnect();
    market_data.stop();
    server.stop();
}
