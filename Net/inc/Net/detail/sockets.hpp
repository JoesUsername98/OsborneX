#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace OsborneX::Net::detail {

#if defined(_WIN32)
using NativeSocket = SOCKET;
inline constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
inline constexpr NativeSocket kInvalidSocket = -1;
#endif

/// @brief Idempotent, process-wide network stack init (WSAStartup on Windows; a
///        no-op on POSIX). Safe to call from any thread, any number of times.
void ensure_network_initialized();

void close_native_socket(NativeSocket socket) noexcept;

/// @brief Blocks up to @p timeout_ms waiting for @p socket to become readable.
/// @details Used instead of a plain blocking recv()/accept() so worker threads can
///          poll an atomic "running" flag between waits and stop promptly, rather
///          than risk the cross-thread close()-while-blocked-in-recv() race that a
///          shutdown-from-another-thread approach would require reasoning about.
bool wait_readable(NativeSocket socket, int timeout_ms);

} // namespace OsborneX::Net::detail
