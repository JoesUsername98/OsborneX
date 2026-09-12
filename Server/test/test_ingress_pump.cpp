#include <Server/ingress_pump.hpp>

#include <thread>
#include <vector>

#include <Simulation/simulation.hpp>
#include <TestSupport/wait_for.hpp>
#include <gtest/gtest.h>

using namespace OsborneX;
using OsborneX::TestSupport::wait_for;

// Real multithreaded test: many producer threads pushing into the MpscQueue while
// one IngressPump thread drains into a real Simulation, exercising exactly the
// concurrency surface IngressPump exists for.

TEST(IngressPumpTest, DrainsConcurrentlyPushedMessagesOnASingleThreadIntoSimulation)
{
    constexpr int producer_count = 4;
    constexpr int orders_per_producer = 50;

    Simulation::Simulation simulation{ 2 };
    Queue::MpscQueue<Simulation::OrderMessage> inbound(64);
    Server::IngressPump pump(inbound, simulation);

    simulation.start();
    pump.start();

    std::vector<std::thread> producers;
    for (int p = 0; p < producer_count; ++p)
    {
        producers.emplace_back([&, p] {
            const auto symbol = static_cast<Simulation::SymbolId>(p);
            for (int i = 0; i < orders_per_producer; ++i)
            {
                inbound.push(Simulation::OrderMessage{
                    .symbol = symbol,
                    .order_id = static_cast<Simulation::OrderId>(p * orders_per_producer + i),
                    .side = Simulation::Side::Buy,
                    .price = 100.0 + static_cast<double>(i),
                    .quantity = 1,
                    .type = Simulation::OrderType::GoodTillCancel,
                    .action = Simulation::OrderAction::Add,
                });
            }
        });
    }
    for (auto& producer : producers)
        producer.join();

    for (int p = 0; p < producer_count; ++p)
    {
        const auto symbol = static_cast<Simulation::SymbolId>(p);
        const auto shard_index = Simulation::Router::shard_for_symbol(symbol, simulation.shard_count());
        ASSERT_TRUE(wait_for([&] {
            return simulation.shard(shard_index).book_size(symbol) == static_cast<std::size_t>(orders_per_producer);
        })) << "symbol " << symbol;
    }

    pump.stop();
    simulation.stop();
}
