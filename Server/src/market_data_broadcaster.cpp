#include <Server/market_data_broadcaster.hpp>

#include <cstddef>

namespace OsborneX::Server {
namespace {

Simulation::Subscriber::Handler MakeTopOfBookHandler(Net::MarketDataPublisherUdp& udp)
{
    return [&udp](const Simulation::TopOfBookUpdate& update) { udp.send_top_of_book(update); };
}

TradeStreamReader::Handler MakeTradeHandler(Net::MarketDataPublisherUdp& udp)
{
    return [&udp](const Simulation::TradeExecution& trade) { udp.send_trade(trade); };
}

} // namespace

MarketDataBroadcaster::MarketDataBroadcaster(Net::MarketDataPublisherUdp& udp, Simulation::Simulation& simulation)
    : top_of_book_subscriber_{ MakeTopOfBookHandler(udp) }
    , trade_reader_{ MakeTradeHandler(udp) }
{
    simulation.add_subscriber(top_of_book_subscriber_);
    for (std::size_t i = 0; i < simulation.shard_count(); ++i)
        trade_reader_.register_producer(simulation.shard(i).trade_out());
}

void MarketDataBroadcaster::start()
{
    top_of_book_subscriber_.start();
    trade_reader_.start();
}

void MarketDataBroadcaster::stop()
{
    top_of_book_subscriber_.stop();
    trade_reader_.stop();
}

} // namespace OsborneX::Server
