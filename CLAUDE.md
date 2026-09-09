# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

OsborneX is a C++23 limit order book, built as a concurrency-practice project. `Orderbook` is a single-threaded matching engine; everything else is a small set of focused libraries that together shard multiple `Orderbook` instances across worker threads, feed them from a single sequencing point, and fan out top-of-book updates to subscribers:

- **Orderbook** — single-threaded, single-symbol matching engine. Has no concurrency awareness at all.
- **Messages** — shared wire types (`OrderMessage`, `TopOfBookUpdate`, …) and the `OrderSink` interface. Header-only; everything else depends on it.
- **Queue** — generic `RingBuffer<T>` (Disruptor-style: single producer, independent per-consumer cursors). No domain knowledge, own namespace `OsborneX::Queue`.
- **Ingress** — sequences/timestamps incoming orders and forwards them through an `OrderSink&`. Knows nothing about `Sharding`.
- **Sharding** — `Router` (symbol → shard hashing) and `Shard` (one worker thread + private `Orderbook`s). Knows nothing about `PubSub`.
- **PubSub** — `MarketDataPublisher` (setup-time wiring registry) and `Subscriber` (pulls from registered producer buffers). Knows nothing about `Sharding`.
- **Simulation** — thin facade that wires the above together and owns their lifecycle.
- **TestSupport** — test-only `wait_for(predicate)` helper; linked by no production target.

`Ingress`, `Sharding`, and `PubSub` are mutually independent — none includes another's headers. Only `Simulation` depends on all of them.

## Build

Cross-platform builds use [CMake presets](CMakePresets.json) (CMake ≥ 3.25, C++23).

```powershell
cmake --list-presets
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
```

Release presets: `windows-msvc-release` / `linux-gcc-release`. Build output goes to `build/<preset-name>/`. Windows uses the VS 2026 generator; both toolchains build warnings-as-errors (`/W4 /WX /permissive-` on MSVC, `-Wall -Wextra -Wpedantic -Werror` otherwise — see `cmake/CompilerWarnings.cmake`).

Third-party deps (googletest, google/benchmark) are pulled via `FetchContent` in `cmake/Dependencies.cmake` — no manual vendoring needed. There is no other runtime dependency: `Queue::RingBuffer` is hand-rolled (no moodycamel or other lock-free library).

## Tests

Every library has its own GoogleTest binary, discovered via `gtest_discover_tests`: `OrderbookTest`, `QueueTest`, `IngressTest`, `ShardingTest`, `PubSubTest`, `SimulationTest`.

```powershell
ctest --preset windows-msvc-debug
```

```bash
ctest --preset linux-gcc-debug
```

To run a single test, invoke the binary directly with a GTest filter:

```powershell
.\build\windows-msvc-debug\Orderbook\Debug\OrderbookTest.exe --gtest_filter=SuiteName.TestName
.\build\windows-msvc-debug\Simulation\Debug\SimulationTest.exe --gtest_filter=SuiteName.TestName
```

```bash
./build/linux-gcc-debug/Orderbook/OrderbookTest --gtest_filter=SuiteName.TestName
./build/linux-gcc-debug/Simulation/SimulationTest --gtest_filter=SuiteName.TestName
```

Convenience targets: `cmake --build --preset <preset> --target run-tests` / `run-benchmarks`.

**Concurrency testing philosophy** (in the spirit of *C++ Concurrency in Action* ch. 11): multithreaded tests use `TestSupport::wait_for(predicate, timeout)` to poll for a condition instead of a fixed `sleep_for` guess — this replaces every prior sleep-based wait in the suite. `Queue/test/test_ring_buffer.cpp` drives the ring buffer's producer/consumer API directly from a single thread to test sequence/gating/catch-up math deterministically; `Queue/test/test_ring_buffer_stress.cpp` and the `*Concurrency*`-named tests use real threads with hard conservation invariants (e.g. `items_read + dropped_count() == produced_count()`). No sanitizers or CI are configured — rigor here comes from test design, not tooling. Stress suites are opt-in, not part of the default `ctest` run:

```powershell
.\build\windows-msvc-debug\Queue\Debug\QueueTest.exe --gtest_filter=*Stress* --gtest_repeat=200
.\build\windows-msvc-debug\Simulation\Debug\SimulationTest.exe --gtest_filter=*Concurrency* --gtest_repeat=50
```

## Benchmarks

Google Benchmark binary lives under `Orderbook/bench`:

```powershell
.\build\windows-msvc-debug\Orderbook\Debug\OrderbookBench.exe
```

```bash
./build/linux-gcc-debug/Orderbook/OrderbookBench
```

## Architecture

### Orderbook (`Orderbook/`)

Single-threaded matching engine, namespace `OsborneX`. Public headers in `inc/Orderbook/`, implementation in `src/orderbook.cpp`.

- `Orderbook` (`orderbook.hpp`/`.cpp`) holds bids/asks as `std::map<Price, OrderPointers>` (bids sorted descending, asks ascending, `OrderPointers = std::list<std::shared_ptr<Order>>` for O(1) cancel-by-iterator), plus an `orders_` map from `OrderId` to `{order, list iterator}` for O(1) lookup/cancel, and a `levelData_` map tracking aggregate quantity/order-count per price level (used by `CanFullyFill` for `FillOrKill` checks).
- `AddOrder` handles order-type-specific admission logic first (`FillAndKill` requires an immediate match; `Market` orders adopt the worst opposing price and convert to `GoodTillCancel` via `Order::ToGoodTillCancel`; `FillOrKill` requires `CanFullyFill` across price levels) before inserting and calling `MatchOrders()`.
- `MatchOrders()` walks best bid vs. best ask while they cross, fills at the resting order's price, removes fully-filled orders, and cancels any newly-exposed `FillAndKill` order at the top of book after each price-level exhausts.
- `ModifyOrder` is implemented as cancel + re-add (`orderbook.cpp:178`), which changes the order's queue priority.
- `GetNextMarketClose` uses `std::chrono` zoned time against `marketCloseHour_`; it does not account for weekends/holidays (every day is treated as a trading day) and interprets the close hour in the system's local timezone.
- Order semantics live in `order.hpp`: `Fill`/`ToGoodTillCancel` currently swallow error cases silently (commented-out `std::expected` returns) rather than surfacing them — be aware of this when changing fill/validation logic.
- `test/test_lifecycle.cpp` (renamed from `test_threading.cpp`) is a plain single-threaded construct/destroy check — `Orderbook` itself has no threading to test. Real "one `Orderbook`, one thread" coverage lives in `Sharding/test/test_shard.cpp`.

### `Queue::RingBuffer<T>` (`Queue/`)

Namespace `OsborneX::Queue` (deliberately outside `OsborneX::Simulation` — it's a generic primitive with no domain knowledge). Header-only, `Queue/inc/Queue/ring_buffer.hpp`.

- Modeled on the LMAX Disruptor: one pre-allocated, power-of-two-sized buffer, a single producer, and any number of independent consumer cursors (`RingBuffer::Consumer`) — no per-consumer queue, no allocation on the hot path.
- Producer API is either `push_blocking`/`claim_blocking` (spins until every `Lossless` consumer has freed a slot — order flow must never drop) or `push_overwrite`/`claim_overwrite` (never blocks — market data must never stall a shard's hot path). **One buffer type serves both edges; the difference is entirely per-consumer policy** (`ConsumerPolicy::Lossless` vs `::Lossy`), not two queue classes.
- A `Lossy` consumer that falls more than `capacity()` behind jumps forward to `produced - capacity()` and counts the skip in `dropped_count()`. A seqlock-style pre/post check around the payload copy in `Consumer::try_read` catches a slot being overwritten mid-read; on the `Lossless` path this is `assert`-guarded as structurally unreachable (a hard signal if producer gating is ever wrong).
- Single-producer discipline is enforced by a debug-only `assert` (recorded thread-id on first `claim_*` call), not the type system. `reset_producer_thread()` exists specifically for a *provably safe* hand-off (e.g. after `std::thread::join()`) — see `Shard::start()`/`stop()`, which call it because `Shard::drain()` runs `process()` (and therefore `outbound_.push_overwrite`) on the calling thread after the worker thread has already stopped.
- No sanitizers are used to validate this; correctness is argued via `Queue/test/test_ring_buffer.cpp` (single-threaded, drives the API directly to hit gating/wraparound/catch-up/torn-read edge cases deterministically) and `Queue/test/test_ring_buffer_stress.cpp` (real threads, conservation invariants, meant to be run with `--gtest_repeat`).

### Ingress (`Ingress/`)

`Ingress::receive(OrderMessage)` stamps a steady-clock timestamp and a monotonically increasing `IngressSequence`, then forwards via `OrderSink&` (implemented by `Sharding::Router`) — `Ingress` has no dependency on `Sharding` at all. Single-caller-thread discipline is enforced by a debug-only `assert`, same pattern as `RingBuffer`.

### Sharding (`Sharding/`)

- **`Router`** (`OrderSink` implementation) deterministically maps a symbol to a shard via `hash(SymbolId) % shard_count` (`Router::shard_for_symbol`) and pushes onto that shard's inbound `RingBuffer`.
- **`Shard`** owns one worker thread, a private `std::unordered_map<SymbolId, Orderbook>` (no locking needed — each shard is single-threaded internally), an inbound `RingBuffer<OrderMessage>` (one `Lossless` consumer: its own worker thread), and its own outbound `RingBuffer<TopOfBookUpdate>` (`market_data_out()`) that it publishes into via `push_overwrite` whenever the top of book actually changes (`top_of_book_equal`, NaN-aware via `SamePrice`). `Shard` has no dependency on `PubSub` — whoever wants its market data registers their own consumer cursor directly against `market_data_out()`.

### PubSub (`PubSub/`)

- **`MarketDataPublisher`** is a setup-time-only wiring registry (`add_subscriber`/`add_producer`/`freeze()`) that cross-registers every subscriber against every producer's buffer, in whichever order they're added. It does no work on any hot path — this replaces the old design's unsynchronized `vector<Subscriber*>`, closing that race by construction rather than documentation. `freeze()` (debug-asserted) must be called once, after wiring and before any producer thread starts.
- **`Subscriber`** holds one `RingBuffer<TopOfBookUpdate>::Consumer` (always `Lossy`) per registered producer and round-robins across them on its own worker thread. `dropped_count()` sums drops across all its cursors. Because "dropped" is now consumer-observed (computed lazily inside `try_read`), a subscriber that never reads never reports drops — a real semantic shift from the old push-based queue, where a drop was counted at push time regardless of whether anyone was listening (see `DropPolicyTest` in `Simulation/test/test_concurrency.cpp` for how this is tested deterministically: flood a tiny-capacity buffer to completion *before* starting the subscriber, so the overrun doesn't race a live consumer thread).

### Simulation (`Simulation/`)

Thin facade, namespace `OsborneX::Simulation`. `SimulationOptions{shard_count, shard_inbound_capacity, shard_outbound_capacity}` configures shard count and both `RingBuffer` capacities; the `Simulation(std::size_t shard_count)` overload is sugar over `SimulationOptions`. Wires `Router` → `Ingress` via `OrderSink&`, registers each shard's `market_data_out()` with `MarketDataPublisher` at construction, and calls `publisher_.freeze()` inside `start()` before starting shard threads. Symbol-to-shard mapping means all messages for a given symbol are processed in ingress order by a single shard thread — cross-symbol ordering is not guaranteed.
