#include <PubSub/subscriber.hpp>

#include <thread>

namespace OsborneX::Simulation {

Subscriber::Subscriber(Handler handler)
    : handler_(std::move(handler))
{
}

void Subscriber::register_producer(Queue::RingBuffer<TopOfBookUpdate>& buffer)
{
    sources_.push_back(&buffer.add_consumer(Queue::ConsumerPolicy::Lossy));
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

    // Drain any remaining items across all sources on the calling thread,
    // mirroring the old push-based queue's post-stop drain behavior.
    while (poll_all_sources_once())
    {
    }
}

std::uint64_t Subscriber::dropped_count() const
{
    std::uint64_t total = 0;
    for (const auto* source : sources_)
        total += source->dropped_count();
    return total;
}

std::vector<TopOfBookUpdate> Subscriber::snapshot_events() const
{
    std::lock_guard lock{ events_mutex_ };
    return events_;
}

bool Subscriber::poll_all_sources_once()
{
    bool progressed = false;
    TopOfBookUpdate event;
    for (auto* source : sources_)
    {
        const Queue::ReadResult result = source->try_read(event);
        if (result == Queue::ReadResult::Ok)
        {
            handle(event);
            progressed = true;
        }
        else if (result == Queue::ReadResult::Dropped)
        {
            progressed = true;
        }
    }
    return progressed;
}

void Subscriber::run()
{
    while (running_.load(std::memory_order_acquire))
    {
        if (sources_.empty() || !poll_all_sources_once())
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
