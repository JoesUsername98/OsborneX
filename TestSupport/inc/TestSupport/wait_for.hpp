#pragma once

#include <chrono>
#include <thread>

namespace OsborneX::TestSupport {

/// @brief Polls @p predicate until it returns true or @p timeout elapses.
/// @details Replaces fixed sleep_for-based test synchronization with a bounded,
///          clear-failure wait: the caller gets a definite pass/fail rather than
///          a guess about how long a background thread needs to catch up.
template <typename Predicate>
bool wait_for(
    Predicate predicate,
    std::chrono::milliseconds timeout = std::chrono::milliseconds{ 2000 },
    std::chrono::milliseconds poll_interval = std::chrono::milliseconds{ 1 })
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return predicate();
        std::this_thread::sleep_for(poll_interval);
    }
    return true;
}

} // namespace OsborneX::TestSupport
