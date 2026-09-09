#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <thread>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324) // intentional padding: alignas(64) separates hot cache lines by design
#endif

namespace OsborneX::Queue {

enum class ConsumerPolicy
{
    Lossless,
    Lossy
};

enum class ReadResult
{
    Empty,
    Ok,
    Dropped
};

/// @brief Single-producer ring buffer with independent, policy-driven consumer cursors.
/// @details Modeled on the LMAX Disruptor: one pre-allocated buffer, one producer, and any
///          number of consumers each tracking their own read position via memory barriers
///          rather than per-consumer queues. A Lossless consumer gates the producer (never
///          drops); a Lossy consumer never blocks the producer and instead catches up by
///          skipping forward when it falls more than @c capacity() behind, counting the loss.
template <typename T>
class RingBuffer
{
public:
    using Sequence = std::uint64_t;

    explicit RingBuffer(std::size_t capacity)
        : mask_{ std::bit_ceil(capacity == 0 ? std::size_t{ 1 } : capacity) - 1 }
        , slots_(mask_ + 1)
    {
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    std::size_t capacity() const noexcept { return mask_ + 1; }

    Sequence produced_count() const noexcept
    {
        return producer_sequence_.load(std::memory_order_acquire);
    }

    class Consumer
    {
    public:
        // Only RingBuffer can construct a PassKey (its default constructor
        // is private, friended to RingBuffer only), which is what lets
        // Consumer's own constructor be public -- required so that
        // std::deque::emplace_back can construct Consumer in place, since
        // library-internal construction code is not itself a friend of
        // RingBuffer and cannot call a private Consumer constructor
        // directly.
        class PassKey
        {
        private:
            friend class RingBuffer;
            PassKey() = default;
        };

        Consumer(PassKey, RingBuffer& owner, ConsumerPolicy policy)
            : owner_{ owner }
            , policy_{ policy }
        {
        }

        Consumer(const Consumer&) = delete;
        Consumer& operator=(const Consumer&) = delete;

        /// @brief Attempts to read the next item for this cursor. Never blocks.
        ReadResult try_read(T& out)
        {
            Sequence want = sequence_.load(std::memory_order_relaxed);
            const Sequence produced = owner_.producer_sequence_.load(std::memory_order_acquire);
            if (want >= produced)
                return ReadResult::Empty;

            const Sequence capacity = static_cast<Sequence>(owner_.capacity());
            if (policy_ == ConsumerPolicy::Lossy && produced - want > capacity)
            {
                const Sequence target = produced - capacity;
                dropped_.fetch_add(target - want, std::memory_order_relaxed);
                want = target;
            }

            auto& slot = owner_.slots_[want & owner_.mask_];

            if (slot.seq.load(std::memory_order_acquire) != want)
                return report_stale_slot(want);

            out = slot.value;

            if (slot.seq.load(std::memory_order_acquire) != want)
                return report_stale_slot(want);

            sequence_.store(want + 1, std::memory_order_release);
            return ReadResult::Ok;
        }

        std::uint64_t dropped_count() const noexcept
        {
            return dropped_.load(std::memory_order_relaxed);
        }

        /// @brief This cursor's next sequence to read. Useful for deterministic test waits.
        Sequence position() const noexcept { return sequence_.load(std::memory_order_relaxed); }

    private:
        friend class RingBuffer;

        ReadResult report_stale_slot(Sequence want)
        {
            // A Lossless consumer's slot can only go stale if the producer's
            // gating math is broken -- it should be structurally impossible,
            // since claim_blocking() never lets the producer claim a slot
            // this consumer hasn't already freed. Treat it as a hard bug
            // signal rather than silently under-counting drops.
            assert(policy_ == ConsumerPolicy::Lossy &&
                   "Lossless consumer observed a stale/overwritten slot; producer gating is broken");
            dropped_.fetch_add(1, std::memory_order_relaxed);
            sequence_.store(want + 1, std::memory_order_release);
            return ReadResult::Dropped;
        }

        RingBuffer& owner_;
        ConsumerPolicy policy_;
        alignas(64) std::atomic<Sequence> sequence_{ 0 };
        std::atomic<std::uint64_t> dropped_{ 0 };
    };

    /// @brief Registers a new independent consumer cursor. Call before the producer starts.
    /// @return A stable reference (backed by std::deque) valid for the RingBuffer's lifetime.
    Consumer& add_consumer(ConsumerPolicy policy)
    {
        Consumer& consumer = consumers_.emplace_back(typename Consumer::PassKey{}, *this, policy);
        if (policy == ConsumerPolicy::Lossless)
            lossless_consumers_.push_back(&consumer);
        return consumer;
    }

    // --- Producer API: single producer thread only. Debug builds assert
    //     that every call originates from the same thread. ---

    /// @brief Claims the next slot, spinning until every Lossless consumer has freed one.
    Sequence claim_blocking()
    {
        assert_producer_thread();
        const Sequence seq = next_to_claim_;
        const Sequence capacity = static_cast<Sequence>(this->capacity());
        for (Consumer* consumer : lossless_consumers_)
        {
            while (seq - consumer->sequence_.load(std::memory_order_acquire) >= capacity)
                std::this_thread::yield();
        }
        return seq;
    }

    /// @brief Claims the next slot immediately. Never blocks, never fails.
    Sequence claim_overwrite()
    {
        assert_producer_thread();
        return next_to_claim_;
    }

    T& slot(Sequence sequence) noexcept { return slots_[sequence & mask_].value; }

    void publish(Sequence sequence) noexcept
    {
        slots_[sequence & mask_].seq.store(sequence, std::memory_order_release);
        next_to_claim_ = sequence + 1;
        producer_sequence_.store(sequence + 1, std::memory_order_release);
    }

    void push_blocking(T value)
    {
        const Sequence seq = claim_blocking();
        slot(seq) = std::move(value);
        publish(seq);
    }

    void push_overwrite(T value)
    {
        const Sequence seq = claim_overwrite();
        slot(seq) = std::move(value);
        publish(seq);
    }

    /// @brief Releases the recorded producer-thread identity, permitting a different
    ///        thread to become the producer on the next claim.
    /// @details Only safe once the previous producer thread has fully and provably
    ///          stopped calling the producer API -- e.g. after joining it, since
    ///          std::thread::join() establishes happens-before. This exists for
    ///          controlled hand-off (a worker thread stops; the stopping thread then
    ///          drains/publishes any remainder), not as a way to permit genuinely
    ///          concurrent multi-producer use, which this type does not support.
    void reset_producer_thread() noexcept
    {
#ifndef NDEBUG
        producer_thread_.reset();
#endif
    }

private:
    struct alignas(64) Slot
    {
        std::atomic<Sequence> seq{ 0 };
        T value{};
    };

    void assert_producer_thread()
    {
#ifndef NDEBUG
        const auto id = std::this_thread::get_id();
        if (!producer_thread_.has_value())
            producer_thread_ = id;
        assert(*producer_thread_ == id && "RingBuffer producer API called from more than one thread");
#endif
    }

    std::size_t mask_;
    std::vector<Slot> slots_;
    Sequence next_to_claim_{ 0 }; // producer-thread-confined; plain, non-atomic
    alignas(64) std::atomic<Sequence> producer_sequence_{ 0 };
    std::deque<Consumer> consumers_; // stable references on insertion at the end
    std::vector<Consumer*> lossless_consumers_;
#ifndef NDEBUG
    std::optional<std::thread::id> producer_thread_;
#endif
};

} // namespace OsborneX::Queue

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
