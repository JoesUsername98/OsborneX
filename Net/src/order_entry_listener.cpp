#include <Net/order_entry_listener.hpp>

#include <algorithm>

#include <Net/wire/frame.hpp>
#include <Net/wire/order_entry_wire.hpp>

namespace OsborneX::Net {
namespace {

/// @brief Reads exactly @p length bytes, polling @p running between waits so a
///        stop() request is noticed within one poll interval instead of blocking
///        forever in recv() on a peer that never sends anything else.
bool recv_all(detail::NativeSocket socket, void* data, std::size_t length, const std::atomic<bool>& running)
{
    auto* bytes = static_cast<char*>(data);
    std::size_t received = 0;
    while (received < length)
    {
        if (!running.load(std::memory_order_acquire))
            return false;
        if (!detail::wait_readable(socket, 100))
            continue;

        const auto remaining = static_cast<int>(length - received);
#if defined(_WIN32)
        const int result = ::recv(socket, bytes + received, remaining, 0);
#else
        const auto result = ::recv(socket, bytes + received, static_cast<std::size_t>(remaining), 0);
#endif
        if (result <= 0)
            return false;
        received += static_cast<std::size_t>(result);
    }
    return true;
}

} // namespace

OrderEntryListener::OrderEntryListener(std::uint16_t port, MessageHandler handler)
    : port_{ port }
    , handler_{ std::move(handler) }
{
    detail::ensure_network_initialized();
}

OrderEntryListener::~OrderEntryListener()
{
    stop();
}

bool OrderEntryListener::start()
{
    if (running_.exchange(true))
        return true;

    const detail::NativeSocket handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == detail::kInvalidSocket)
    {
        running_.store(false);
        return false;
    }
    listen_socket_.reset(handle);

#if defined(_WIN32)
    const char reuse = 1;
#else
    const int reuse = 1;
#endif
    ::setsockopt(listen_socket_.native(), SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (::bind(listen_socket_.native(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        listen_socket_.reset();
        running_.store(false);
        return false;
    }
    if (::listen(listen_socket_.native(), SOMAXCONN) != 0)
    {
        listen_socket_.reset();
        running_.store(false);
        return false;
    }

    sockaddr_in bound_address{};
#if defined(_WIN32)
    int address_length = sizeof(bound_address);
#else
    socklen_t address_length = sizeof(bound_address);
#endif
    if (::getsockname(listen_socket_.native(), reinterpret_cast<sockaddr*>(&bound_address), &address_length) == 0)
        bound_port_.store(ntohs(bound_address.sin_port), std::memory_order_release);

    accept_thread_ = std::thread(&OrderEntryListener::accept_loop, this);
    return true;
}

std::uint16_t OrderEntryListener::local_port() const noexcept
{
    return bound_port_.load(std::memory_order_acquire);
}

void OrderEntryListener::stop()
{
    if (!running_.exchange(false))
        return;

    if (accept_thread_.joinable())
        accept_thread_.join();

    std::vector<Connection> connections;
    {
        std::lock_guard lock{ connections_mutex_ };
        connections.swap(connections_);
    }
    for (auto& connection : connections)
        if (connection.thread.joinable())
            connection.thread.join();

    listen_socket_.reset();
}

void OrderEntryListener::reap_finished_connections()
{
    // Called with connections_mutex_ held. A finished connection_loop has already
    // returned, so join() here is immediate -- this just reclaims the thread
    // object rather than leaving it in the vector until stop().
    const auto first_finished = std::remove_if(connections_.begin(), connections_.end(), [](Connection& c) {
        return c.finished->load(std::memory_order_acquire);
    });
    for (auto it = first_finished; it != connections_.end(); ++it)
        if (it->thread.joinable())
            it->thread.join();
    connections_.erase(first_finished, connections_.end());
}

void OrderEntryListener::accept_loop()
{
    while (running_.load(std::memory_order_acquire))
    {
        if (!detail::wait_readable(listen_socket_.native(), 100))
            continue;

        const detail::NativeSocket client = ::accept(listen_socket_.native(), nullptr, nullptr);
        if (client == detail::kInvalidSocket)
            continue; // spurious wake, or racing stop() closing the listen socket

        auto finished = std::make_shared<std::atomic<bool>>(false);
        std::lock_guard lock{ connections_mutex_ };
        reap_finished_connections();
        connections_.push_back(Connection{
            std::thread(&OrderEntryListener::connection_loop, this, SocketHandle{ client }, finished),
            finished,
        });
    }
}

void OrderEntryListener::connection_loop(SocketHandle client, std::shared_ptr<std::atomic<bool>> finished)
{
    while (running_.load(std::memory_order_acquire))
    {
        wire::FrameHeader header{};
        if (!recv_all(client.native(), &header, sizeof(header), running_))
            break;
        if (header.magic != wire::kFrameMagic || header.version != wire::kFrameVersion)
            break; // not a peer speaking this protocol -- drop rather than misinterpret
        if (header.message_type != wire::MessageType::OrderEntry ||
            header.payload_length != sizeof(wire::OrderEntryWire))
            break;

        wire::OrderEntryWire payload{};
        if (!recv_all(client.native(), &payload, sizeof(payload), running_))
            break;

        handler_(wire::decode(payload));
    }
    finished->store(true, std::memory_order_release);
}

} // namespace OsborneX::Net
