#pragma once

#include <vector>

#include <Queue/ring_buffer.hpp>
#include <Messages/types.hpp>

namespace OsborneX::Simulation {

class Subscriber;

/// @brief Setup-time wiring registry cross-connecting shard outbound buffers to subscribers.
/// @details Unlike the old push-based publisher, this does no work on any hot path: each
///          producer (e.g. Shard) owns its own outbound RingBuffer and each Subscriber reads
///          directly from the buffers it's registered against. This class only exists to let
///          callers wire "these producers feed these subscribers" once, at setup time, before
///          any producer thread starts -- closing the old unsynchronized-subscriber-list race
///          by construction rather than by documentation.
class MarketDataPublisher
{
public:
    void add_subscriber(Subscriber& subscriber);
    void add_producer(Queue::RingBuffer<TopOfBookUpdate>& producer);

    /// Closes the topology. Call once, after all subscribers/producers are
    /// registered and before any producer thread starts.
    void freeze();

private:
    bool frozen_{ false };
    std::vector<Subscriber*> subscribers_;
    std::vector<Queue::RingBuffer<TopOfBookUpdate>*> producers_;
};

} // namespace OsborneX::Simulation
