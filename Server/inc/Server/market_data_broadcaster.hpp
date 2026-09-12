#pragma once

#include <Net/market_data_multicast.hpp>
#include <PubSub/subscriber.hpp>
#include <Server/trade_stream_reader.hpp>
#include <Simulation/simulation.hpp>

namespace OsborneX::Server {

/// @brief Bridges each shard's outbound buffers to the UDP multicast publisher.
/// @details Reuses PubSub::Subscriber as-is for the TopOfBookUpdate half (its existing
///          Handler callback already does exactly "serialize and send on delivery",
///          invoked on the subscriber's own thread) -- zero PubSub changes needed. The
///          TradeExecution half has no PubSub equivalent, so it uses the small dedicated
///          TradeStreamReader instead of templating PubSub for one caller.
class MarketDataBroadcaster
{
public:
    MarketDataBroadcaster(Net::MarketDataPublisherUdp& udp, Simulation::Simulation& simulation);

    MarketDataBroadcaster(const MarketDataBroadcaster&) = delete;
    MarketDataBroadcaster& operator=(const MarketDataBroadcaster&) = delete;

    void start();
    void stop();

private:
    Simulation::Subscriber top_of_book_subscriber_;
    TradeStreamReader trade_reader_;
};

} // namespace OsborneX::Server
