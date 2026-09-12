#include <Net/socket_handle.hpp>

namespace OsborneX::Net {

SocketHandle::~SocketHandle()
{
    reset();
}

SocketHandle::SocketHandle(SocketHandle&& other) noexcept
    : handle_{ other.handle_ }
{
    other.handle_ = detail::kInvalidSocket;
}

SocketHandle& SocketHandle::operator=(SocketHandle&& other) noexcept
{
    if (this != &other)
    {
        reset();
        handle_ = other.handle_;
        other.handle_ = detail::kInvalidSocket;
    }
    return *this;
}

void SocketHandle::reset(detail::NativeSocket handle) noexcept
{
    if (handle_ != detail::kInvalidSocket)
        detail::close_native_socket(handle_);
    handle_ = handle;
}

} // namespace OsborneX::Net
