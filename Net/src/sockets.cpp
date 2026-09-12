#include <Net/detail/sockets.hpp>

#include <mutex>

namespace OsborneX::Net::detail {
namespace {
std::once_flag g_init_flag;
}

void ensure_network_initialized()
{
#if defined(_WIN32)
    std::call_once(g_init_flag, [] {
        WSADATA data{};
        ::WSAStartup(MAKEWORD(2, 2), &data);
    });
#endif
}

void close_native_socket(NativeSocket socket) noexcept
{
#if defined(_WIN32)
    ::closesocket(socket);
#else
    ::close(socket);
#endif
}

bool wait_readable(NativeSocket socket, int timeout_ms)
{
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket, &read_set);

    timeval timeout{};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

#if defined(_WIN32)
    const int result = ::select(0, &read_set, nullptr, nullptr, &timeout);
#else
    const int result = ::select(socket + 1, &read_set, nullptr, nullptr, &timeout);
#endif
    return result > 0;
}

} // namespace OsborneX::Net::detail
