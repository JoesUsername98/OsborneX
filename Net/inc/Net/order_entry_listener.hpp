#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <Messages/types.hpp>
#include <Net/socket_handle.hpp>

namespace OsborneX::Net {

/// @brief TCP server accepting order-entry connections: one accept loop plus one
///        reader thread per connection, each decoding frames and invoking a handler.
/// @details Deliberately ignorant of Simulation/Ingress -- the handler is just a
///          callback, invoked on whichever connection thread decoded the message.
///          Funneling those onto the single thread allowed to call Simulation::submit()
///          is the caller's job (see Server::IngressPump), keeping Net independent of
///          Simulation per the project's one-directional dependency rules.
class OrderEntryListener
{
public:
    using MessageHandler = std::function<void(Simulation::OrderMessage)>;

    OrderEntryListener(std::uint16_t port, MessageHandler handler);
    ~OrderEntryListener();

    OrderEntryListener(const OrderEntryListener&) = delete;
    OrderEntryListener& operator=(const OrderEntryListener&) = delete;

    /// @brief Binds and starts accepting connections on a background thread.
    /// @return false if bind/listen failed.
    bool start();
    void stop();

    /// @brief The port actually bound, once start() has succeeded -- resolves a
    ///        constructor port of 0 ("let the OS choose") to its real value, which
    ///        is what test/loopback clients should connect to.
    std::uint16_t local_port() const noexcept;

private:
    /// One accepted connection's worker thread plus a flag it sets just before
    /// returning -- lets accept_loop() reap finished connections opportunistically
    /// instead of connection_threads_ growing for the life of the process.
    struct Connection
    {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };

    void accept_loop();
    void connection_loop(SocketHandle client, std::shared_ptr<std::atomic<bool>> finished);
    void reap_finished_connections();

    std::uint16_t port_;
    std::atomic<std::uint16_t> bound_port_{ 0 };
    MessageHandler handler_;
    SocketHandle listen_socket_;
    std::atomic<bool> running_{ false };
    std::thread accept_thread_;
    std::mutex connections_mutex_;
    std::vector<Connection> connections_;
};

} // namespace OsborneX::Net
