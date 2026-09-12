#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include <Messages/trade_execution.hpp>
#include <Messages/types.hpp>
#include <Net/socket_handle.hpp>

namespace OsborneX::Net {

/// @brief Sends TopOfBookUpdate/TradeExecution as best-effort UDP multicast datagrams.
/// @details One shared multicast group/port carries both message types (discriminated
///          by FrameHeader::message_type) -- simplest to configure across the server,
///          bots, and the TUI, and the datagram volume here is negligible for a
///          practice project, unlike a real bandwidth-constrained exchange feed.
class MarketDataPublisherUdp
{
public:
    MarketDataPublisherUdp(std::string group, std::uint16_t port);

    MarketDataPublisherUdp(const MarketDataPublisherUdp&) = delete;
    MarketDataPublisherUdp& operator=(const MarketDataPublisherUdp&) = delete;

    void send_top_of_book(const Simulation::TopOfBookUpdate& update);
    void send_trade(const Simulation::TradeExecution& trade);

private:
    std::string group_;
    std::uint16_t port_;
    SocketHandle socket_;
};

/// @brief Joins the multicast group and dispatches received datagrams to the matching
///        handler based on FrameHeader::message_type.
class MarketDataListener
{
public:
    using TopOfBookHandler = std::function<void(const Simulation::TopOfBookUpdate&)>;
    using TradeHandler = std::function<void(const Simulation::TradeExecution&)>;

    MarketDataListener(std::string group, std::uint16_t port, TopOfBookHandler on_top, TradeHandler on_trade);
    ~MarketDataListener();

    MarketDataListener(const MarketDataListener&) = delete;
    MarketDataListener& operator=(const MarketDataListener&) = delete;

    bool start();
    void stop();

private:
    void run();

    std::string group_;
    std::uint16_t port_;
    TopOfBookHandler on_top_;
    TradeHandler on_trade_;
    SocketHandle socket_;
    std::atomic<bool> running_{ false };
    std::thread thread_;
};

} // namespace OsborneX::Net
