#include <Server/trade_stream_reader.hpp>

namespace OsborneX::Server {

TradeStreamReader::TradeStreamReader(Handler handler)
    : handler_{ std::move(handler) }
{
}

void TradeStreamReader::register_producer(Queue::RingBuffer<Simulation::TradeExecution>& buffer)
{
    sources_.push_back(&buffer.add_consumer(Queue::ConsumerPolicy::Lossy));
}

void TradeStreamReader::start()
{
    if (running_.exchange(true))
        return;
    thread_ = std::thread(&TradeStreamReader::run, this);
}

void TradeStreamReader::stop()
{
    if (!running_.exchange(false))
        return;

    if (thread_.joinable())
        thread_.join();

    // Drain any remaining items across all sources on the calling thread,
    // mirroring PubSub::Subscriber's post-stop drain behavior.
    while (poll_all_sources_once())
    {
    }
}

bool TradeStreamReader::poll_all_sources_once()
{
    bool progressed = false;
    Simulation::TradeExecution trade;
    for (auto* source : sources_)
    {
        const Queue::ReadResult result = source->try_read(trade);
        if (result == Queue::ReadResult::Ok)
        {
            if (handler_)
                handler_(trade);
            progressed = true;
        }
        else if (result == Queue::ReadResult::Dropped)
        {
            progressed = true;
        }
    }
    return progressed;
}

void TradeStreamReader::run()
{
    while (running_.load(std::memory_order_acquire))
    {
        if (sources_.empty() || !poll_all_sources_once())
            std::this_thread::yield();
    }
}

} // namespace OsborneX::Server
