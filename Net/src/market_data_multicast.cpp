#include <Net/market_data_multicast.hpp>

#include <cstring>
#include <stdexcept>

#include <Net/wire/frame.hpp>
#include <Net/wire/market_data_wire.hpp>

namespace OsborneX::Net {

// --- MarketDataPublisherUdp -------------------------------------------------

MarketDataPublisherUdp::MarketDataPublisherUdp(std::string group, std::uint16_t port)
    : group_{ std::move(group) }
    , port_{ port }
{
    detail::ensure_network_initialized();

    const detail::NativeSocket handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == detail::kInvalidSocket)
        throw std::runtime_error("MarketDataPublisherUdp: failed to create UDP socket");
    socket_.reset(handle);

    // TTL failing to set is non-fatal -- it only affects how many multicast hops a
    // datagram survives, and the socket remains usable with whatever TTL the
    // platform defaults to.
    const int ttl = 8;
    ::setsockopt(socket_.native(), IPPROTO_IP, IP_MULTICAST_TTL,
                 reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    if (::inet_pton(AF_INET, group_.c_str(), &address.sin_addr) != 1)
        throw std::runtime_error("MarketDataPublisherUdp: invalid multicast group address '" + group_ + "'");

    // Connecting a UDP socket just fixes the default destination for send(); it does
    // not establish a stream connection.
    if (::connect(socket_.native(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
        throw std::runtime_error("MarketDataPublisherUdp: failed to set default destination " + group_);
}

void MarketDataPublisherUdp::send_top_of_book(const Simulation::TopOfBookUpdate& update)
{
    const wire::TopOfBookUpdateWire payload = wire::encode(update);
    wire::FrameHeader header{};
    header.message_type = wire::MessageType::TopOfBook;
    header.payload_length = static_cast<std::uint16_t>(sizeof(payload));

    char buffer[sizeof(header) + sizeof(payload)];
    std::memcpy(buffer, &header, sizeof(header));
    std::memcpy(buffer + sizeof(header), &payload, sizeof(payload));
    ::send(socket_.native(), buffer, static_cast<int>(sizeof(buffer)), 0);
}

void MarketDataPublisherUdp::send_trade(const Simulation::TradeExecution& trade)
{
    const wire::TradeExecutionWire payload = wire::encode(trade);
    wire::FrameHeader header{};
    header.message_type = wire::MessageType::TradeExecution;
    header.payload_length = static_cast<std::uint16_t>(sizeof(payload));

    char buffer[sizeof(header) + sizeof(payload)];
    std::memcpy(buffer, &header, sizeof(header));
    std::memcpy(buffer + sizeof(header), &payload, sizeof(payload));
    ::send(socket_.native(), buffer, static_cast<int>(sizeof(buffer)), 0);
}

// --- MarketDataListener ------------------------------------------------------

MarketDataListener::MarketDataListener(
    std::string group, std::uint16_t port, TopOfBookHandler on_top, TradeHandler on_trade)
    : group_{ std::move(group) }
    , port_{ port }
    , on_top_{ std::move(on_top) }
    , on_trade_{ std::move(on_trade) }
{
    detail::ensure_network_initialized();
}

MarketDataListener::~MarketDataListener()
{
    stop();
}

bool MarketDataListener::start()
{
    if (running_.exchange(true))
        return true;

    const detail::NativeSocket handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == detail::kInvalidSocket)
    {
        running_.store(false);
        return false;
    }
    socket_.reset(handle);

#if defined(_WIN32)
    const char reuse = 1;
#else
    const int reuse = 1;
#endif
    ::setsockopt(socket_.native(), SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    if (::bind(socket_.native(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        socket_.reset();
        running_.store(false);
        return false;
    }

    ip_mreq membership{};
    ::inet_pton(AF_INET, group_.c_str(), &membership.imr_multiaddr);
    membership.imr_interface.s_addr = INADDR_ANY;
    if (::setsockopt(socket_.native(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      reinterpret_cast<const char*>(&membership), sizeof(membership)) != 0)
    {
        socket_.reset();
        running_.store(false);
        return false;
    }

    thread_ = std::thread(&MarketDataListener::run, this);
    return true;
}

void MarketDataListener::stop()
{
    if (!running_.exchange(false))
        return;
    if (thread_.joinable())
        thread_.join();
    socket_.reset();
}

void MarketDataListener::run()
{
    while (running_.load(std::memory_order_acquire))
    {
        if (!detail::wait_readable(socket_.native(), 100))
            continue;

        char buffer[512];
#if defined(_WIN32)
        const int received = ::recv(socket_.native(), buffer, sizeof(buffer), 0);
#else
        const auto received = ::recv(socket_.native(), buffer, sizeof(buffer), 0);
#endif
        if (received <= 0)
            continue;

        wire::FrameHeader header{};
        if (static_cast<std::size_t>(received) < sizeof(header))
            continue;
        std::memcpy(&header, buffer, sizeof(header));

        if (header.magic != wire::kFrameMagic || header.version != wire::kFrameVersion)
            continue; // stray/foreign datagram on the group -- ignore, not a protocol frame

        const char* payload_bytes = buffer + sizeof(header);
        const auto payload_length = static_cast<std::size_t>(received) - sizeof(header);

        if (header.message_type == wire::MessageType::TopOfBook &&
            payload_length >= sizeof(wire::TopOfBookUpdateWire))
        {
            wire::TopOfBookUpdateWire payload{};
            std::memcpy(&payload, payload_bytes, sizeof(payload));
            if (on_top_)
                on_top_(wire::decode(payload));
        }
        else if (header.message_type == wire::MessageType::TradeExecution &&
                 payload_length >= sizeof(wire::TradeExecutionWire))
        {
            wire::TradeExecutionWire payload{};
            std::memcpy(&payload, payload_bytes, sizeof(payload));
            if (on_trade_)
                on_trade_(wire::decode(payload));
        }
    }
}

} // namespace OsborneX::Net
