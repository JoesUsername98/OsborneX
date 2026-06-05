#include <Orderbook/order_book.hpp>

#include <benchmark/benchmark.h>

namespace osbornex {

static void BM_OrderBookEmpty(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(true);
    }
}

BENCHMARK(BM_OrderBookEmpty);

} // namespace osbornex

BENCHMARK_MAIN();
