#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include <Queue/ring_buffer.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Simulation {

class Subscriber
{
public:
    using Handler = std::function<void(const TopOfBookUpdate&)>;

    explicit Subscriber(Handler handler = {});

    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;

    /// Registers a producer's outbound buffer as a source for this subscriber.
    /// Must be called before start().
    void register_producer(Queue::RingBuffer<TopOfBookUpdate>& buffer);

    void start();
    void stop();

    /// Sum of dropped items across every registered producer's cursor.
    std::uint64_t dropped_count() const;
    std::vector<TopOfBookUpdate> snapshot_events() const;

private:
    void run();
    void handle(const TopOfBookUpdate& event);
    /// Reads at most one item from each registered source. Returns whether
    /// any source yielded an item (Ok or Dropped both count as progress).
    bool poll_all_sources_once();

    std::vector<Queue::RingBuffer<TopOfBookUpdate>::Consumer*> sources_;
    Handler handler_;
    std::atomic<bool> running_{ false };
    std::thread thread_;

    mutable std::mutex events_mutex_;
    std::vector<TopOfBookUpdate> events_;
};

} // namespace OsborneX::Simulation
