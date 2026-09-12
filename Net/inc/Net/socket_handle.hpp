#pragma once

#include <Net/detail/sockets.hpp>

namespace OsborneX::Net {

/// @brief RAII ownership of one native socket handle. Move-only; closes on destruction.
class SocketHandle
{
public:
    SocketHandle() noexcept = default;
    explicit SocketHandle(detail::NativeSocket handle) noexcept
        : handle_{ handle }
    {
    }
    ~SocketHandle();

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SocketHandle(SocketHandle&& other) noexcept;
    SocketHandle& operator=(SocketHandle&& other) noexcept;

    detail::NativeSocket native() const noexcept { return handle_; }
    bool valid() const noexcept { return handle_ != detail::kInvalidSocket; }

    /// Closes the currently-owned handle (if any) and takes ownership of @p handle.
    void reset(detail::NativeSocket handle = detail::kInvalidSocket) noexcept;

private:
    detail::NativeSocket handle_{ detail::kInvalidSocket };
};

} // namespace OsborneX::Net
