#include <Simulation/subscriber.hpp>

#include <thread>

namespace OsborneX::Simulation {

Subscriber::Subscriber(std::size_t queue_capacity, Handler handler)
    : queue_(queue_capacity)
    , handler_(std::move(handler))
{
}

void Subscriber::enqueue(TopOfBookUpdate event)
{
    queue_.try_push(std::move(event));
}

void Subscriber::start()
{
    if (running_.exchange(true))
        return;

    thread_ = std::thread(&Subscriber::run, this);
}

void Subscriber::stop()
{
    if (!running_.exchange(false))
        return;

    if (thread_.joinable())
        thread_.join();

    TopOfBookUpdate event;
    while (queue_.try_pop(event))
        handle(event);
}

std::size_t Subscriber::dropped_count() const
{
    return queue_.dropped_count();
}

std::vector<TopOfBookUpdate> Subscriber::snapshot_events() const
{
    std::lock_guard lock{ events_mutex_ };
    return events_;
}

void Subscriber::run()
{
    while (running_.load(std::memory_order_acquire))
    {
        TopOfBookUpdate event;
        if (queue_.try_pop(event))
            handle(event);
        else
            std::this_thread::yield();
    }
}

void Subscriber::handle(const TopOfBookUpdate& event)
{
    {
        std::lock_guard lock{ events_mutex_ };
        events_.push_back(event);
    }

    if (handler_)
        handler_(event);
}

} // namespace OsborneX::Simulation
