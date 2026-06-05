#include "Orderbook/order_book.hpp"

#include <benchmark/benchmark.h>

namespace osbornex {

static void BM_OrderBookEmpty(benchmark::State& state) {
    OrderBook book;
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.empty());
    }
}

BENCHMARK(BM_OrderBookEmpty);

} // namespace osbornex

BENCHMARK_MAIN();
