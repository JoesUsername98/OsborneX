#include <Server/trade_stream_reader.hpp>

#include <mutex>
#include <vector>

#include <TestSupport/wait_for.hpp>
#include <gtest/gtest.h>

using namespace OsborneX;
using OsborneX::TestSupport::wait_for;

namespace {

class TradeLog
{
public:
    void add(const Simulation::TradeExecution& trade)
    {
        std::lock_guard lock{ mutex_ };
        trades_.push_back(trade);
    }

    std::vector<Simulation::TradeExecution> snapshot() const
    {
        std::lock_guard lock{ mutex_ };
        return trades_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<Simulation::TradeExecution> trades_;
};

Simulation::TradeExecution MakeTrade(Simulation::SymbolId symbol)
{
    return Simulation::TradeExecution{
        .symbol = symbol,
        .bid_order_id = 1,
        .bid_price = 100.0,
        .ask_order_id = 2,
        .ask_price = 100.0,
        .quantity = 5,
    };
}

} // namespace

TEST(TradeStreamReaderTest, DeliversTradesPublishedByARegisteredProducer)
{
    Queue::RingBuffer<Simulation::TradeExecution> buffer(8);
    TradeLog log;
    Server::TradeStreamReader reader([&](const Simulation::TradeExecution& trade) { log.add(trade); });
    reader.register_producer(buffer);
    reader.start();

    buffer.push_overwrite(MakeTrade(1));

    ASSERT_TRUE(wait_for([&] { return log.snapshot().size() == 1; }));
    EXPECT_EQ(log.snapshot().front().symbol, 1u);

    reader.stop();
}

TEST(TradeStreamReaderTest, MergesTradesFromMultipleRegisteredProducers)
{
    Queue::RingBuffer<Simulation::TradeExecution> buffer_a(8);
    Queue::RingBuffer<Simulation::TradeExecution> buffer_b(8);
    TradeLog log;
    Server::TradeStreamReader reader([&](const Simulation::TradeExecution& trade) { log.add(trade); });
    reader.register_producer(buffer_a);
    reader.register_producer(buffer_b);
    reader.start();

    buffer_a.push_overwrite(MakeTrade(1));
    buffer_b.push_overwrite(MakeTrade(2));

    ASSERT_TRUE(wait_for([&] { return log.snapshot().size() == 2; }));

    reader.stop();
}
