#include <Orderbook/orderbook.hpp>

#include <benchmark/benchmark.h>

namespace OsborneX {

static void BM_OrderBookEmpty(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(true);
    }
}

BENCHMARK(BM_OrderBookEmpty);

} // namespace OsborneX

BENCHMARK_MAIN();
