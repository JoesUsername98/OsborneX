#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include <Simulation/spsc_queue.hpp>
#include <Simulation/types.hpp>

namespace OsborneX::Simulation {

class Subscriber
{
public:
    using Handler = std::function<void(const TopOfBookUpdate&)>;

    explicit Subscriber(std::size_t queue_capacity = 1024, Handler handler = {});

    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;

    void enqueue(TopOfBookUpdate event);
    void start();
    void stop();

    std::size_t dropped_count() const;
    std::vector<TopOfBookUpdate> snapshot_events() const;

private:
    void run();
    void handle(const TopOfBookUpdate& event);

    DroppingMpscQueue<TopOfBookUpdate> queue_;
    Handler handler_;
    std::atomic<bool> running_{ false };
    std::thread thread_;

    mutable std::mutex events_mutex_;
    std::vector<TopOfBookUpdate> events_;
};

} // namespace OsborneX::Simulation
