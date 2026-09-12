#include <Net/order_entry_client.hpp>

#include <string>

#include <Net/wire/frame.hpp>
#include <Net/wire/order_entry_wire.hpp>

namespace OsborneX::Net {
namespace {

bool send_all(detail::NativeSocket socket, const void* data, std::size_t length)
{
    const auto* bytes = static_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < length)
    {
        const auto remaining = static_cast<int>(length - sent);
#if defined(_WIN32)
        const int result = ::send(socket, bytes + sent, remaining, 0);
#else
        const auto result = ::send(socket, bytes + sent, static_cast<std::size_t>(remaining), 0);
#endif
        if (result <= 0)
            return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

} // namespace

OrderEntryClient::OrderEntryClient(std::string host, std::uint16_t port)
    : host_{ std::move(host) }
    , port_{ port }
{
    detail::ensure_network_initialized();
}

bool OrderEntryClient::connect()
{
    disconnect();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* resolved = nullptr;
    const std::string port_str = std::to_string(port_);
    if (::getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &resolved) != 0)
        return false;

    for (addrinfo* candidate = resolved; candidate != nullptr; candidate = candidate->ai_next)
    {
        const detail::NativeSocket handle =
            ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (handle == detail::kInvalidSocket)
            continue;

        if (::connect(handle, candidate->ai_addr, static_cast<int>(candidate->ai_addrlen)) == 0)
        {
            socket_.reset(handle);
            break;
        }
        detail::close_native_socket(handle);
    }
    ::freeaddrinfo(resolved);

    return socket_.valid();
}

void OrderEntryClient::disconnect()
{
    socket_.reset();
}

bool OrderEntryClient::is_connected() const noexcept
{
    return socket_.valid();
}

bool OrderEntryClient::send(const Simulation::OrderMessage& message)
{
    if (!socket_.valid())
        return false;

    const wire::OrderEntryWire payload = wire::encode(message);
    wire::FrameHeader header{};
    header.message_type = wire::MessageType::OrderEntry;
    header.payload_length = static_cast<std::uint16_t>(sizeof(payload));

    if (!send_all(socket_.native(), &header, sizeof(header)))
        return false;
    return send_all(socket_.native(), &payload, sizeof(payload));
}

} // namespace OsborneX::Net
