#pragma once

#include <cstdint>
#include <string>

#include <Messages/types.hpp>
#include <Net/socket_handle.hpp>

namespace OsborneX::Net {

/// @brief TCP client for submitting orders to an OrderEntryListener.
/// @details Deliberately the same class a passive TUI viewer links against but never
///          calls send() from -- keeping a future interactive TUI a small addition
///          (wire an input control to an existing, already-connected client) rather
///          than a re-plumb.
class OrderEntryClient
{
public:
    OrderEntryClient(std::string host, std::uint16_t port);

    OrderEntryClient(const OrderEntryClient&) = delete;
    OrderEntryClient& operator=(const OrderEntryClient&) = delete;
    OrderEntryClient(OrderEntryClient&&) noexcept = default;
    OrderEntryClient& operator=(OrderEntryClient&&) noexcept = default;

    bool connect();
    void disconnect();
    bool is_connected() const noexcept;

    /// @brief Encodes, frames, and blocking-writes one order to the server.
    /// @return false if not connected or the write failed (the connection is then
    ///         considered dead -- call connect() again to retry).
    bool send(const Simulation::OrderMessage& message);

private:
    std::string host_;
    std::uint16_t port_;
    SocketHandle socket_;
};

} // namespace OsborneX::Net
